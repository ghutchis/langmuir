#include "potential.h"
#include "parameters.h"
#include "chargeagent.h"
#include "cubicgrid.h"
#include "world.h"
#include "rand.h"
#include <cmath>

namespace Langmuir
{

Potential::Potential(World &world, QObject *parent)
    : QObject(parent), m_world(world)
{
}

void Potential::setPotentialZero()
{
    qDebug("langmuir: setting potential to zero");
    for(int i = 0; i < m_world.electronGrid().volume(); i++)
    {
        m_world.electronGrid().setPotential(i, 0);
        m_world.holeGrid().setPotential(i, 0);
    }
}

void Potential::setPotentialLinear()
{
    double VL = m_world.parameters().voltageLeft;
    double VR = m_world.parameters().voltageRight;
    double LX = double(m_world.electronGrid().xSize());
    double m  =(VR - VL) / LX;
    double b  = VL;

    qDebug("langmuir: adding a linear potential with slope %.3g V/nm", m);
    for(int i = 0; i < m_world.electronGrid().xSize(); i++)
    {
        for(int j = 0; j < m_world.electronGrid().ySize(); j++)
        {
            for(int k = 0; k < m_world.electronGrid().zSize(); k++)
            {
                int s = m_world.electronGrid().getIndexS(i, j, k);
                double v = m *(i + 0.5)+ b;
                m_world.electronGrid().addToPotential(s, v);
                m_world.holeGrid().addToPotential(s, v);
            }
        }
    }
}

void Potential::setPotentialGate()
{
    if ( m_world.parameters().gridZ == 1 || m_world.parameters().slopeZ == 0 )
    {
        return;
    }

    qDebug("langmuir: adding gate potential with slope %.3g", m_world.parameters().slopeZ);
    for(int i = 0; i < m_world.electronGrid().xSize(); i++)
    {
        for(int j = 0; j < m_world.electronGrid().ySize(); j++)
        {
            for(int k = 0; k < m_world.electronGrid().zSize(); k++)
            {
                int s = m_world.electronGrid().getIndexS(i, j, k);
                double v = m_world.parameters().slopeZ *(k + 0.5);
                m_world.electronGrid().addToPotential(s, v);
                m_world.holeGrid().addToPotential(s, v);
            }
        }
    }
}

void Potential::setPotentialTraps(const QList<int> &trapIDs,
                                  const QList<double> &trapPotentials)
{
    qDebug("langmuir: Potential::setPotentialTraps");

    if ( m_world.numTraps() != 0 )
    {
        qFatal("langmuir: traps have already been created");
    }

    int toBePlacedTotal    = m_world.maxTraps();
    int toBePlacedForced   = trapIDs.size();
    int toBePlacedRandomly = toBePlacedTotal - toBePlacedForced;
    int toBePlacedSeeds    = toBePlacedRandomly
            * m_world.parameters().seedPercentage;
    int toBePlacedGrown    = toBePlacedRandomly - toBePlacedSeeds;

    // This happens when all traps are forced
    if (toBePlacedRandomly <= 0)
    {
        toBePlacedSeeds = 0;
        toBePlacedGrown = 0;
    }

    if (toBePlacedSeeds == 0 && toBePlacedGrown > 0)
    {
        toBePlacedSeeds = toBePlacedGrown;
        toBePlacedGrown = 0;
    }

    qDebug("langmuir: total traps allowed = %d", toBePlacedTotal);
    qDebug("langmuir: traps in checkpoint = %d", toBePlacedForced);
    qDebug("langmuir: traps to be placed randomly = %d", toBePlacedRandomly);
    qDebug("langmuir: traps to act as seeds = %d", toBePlacedSeeds);
    qDebug("langmuir: traps to be grown = %d", toBePlacedGrown);

    if (toBePlacedTotal < 0 ||
            toBePlacedForced < 0 ||
            toBePlacedRandomly < 0 ||
            toBePlacedSeeds < 0 ||
            toBePlacedGrown < 0 ||
            toBePlacedForced > toBePlacedTotal ||
            toBePlacedRandomly > toBePlacedTotal ||
            toBePlacedSeeds > toBePlacedTotal ||
            toBePlacedGrown > toBePlacedTotal)
    {
        qFatal("langmuir: invalid trap parameters");
    }

    if ( toBePlacedForced > 0 &&
         trapPotentials.size() != 0 &&
         trapPotentials.size() != trapIDs.size() )
    {
        qFatal("langmuir: trapPotential and trap arrays different sizes in checkpoint file");
    }

    // We do not alter the grid potential until we are done
    QList<double> potentials;
    QList<int>         traps;

    // First, place the forced traps
    if (toBePlacedForced > 0)
    {
        qDebug("langmuir: placing %d traps in checkpoint file", toBePlacedForced);
        if (trapPotentials.size() != 0)
        {
            for (int i = 0; i < toBePlacedForced; i++)
            {
                traps.push_back(trapIDs.at(i));
                potentials.push_back(trapPotentials.at(i));
            }
        }
        else
        {
            for (int i = 0; i < toBePlacedForced; i++)
            {
                traps.push_back(trapIDs.at(i));
                potentials.push_back(m_world.parameters().trapPotential);
            }
        }
    }

    // Now place traps randomly
    QList<double> randomPotentials;
    QList<int> randomIDs;

    // Place homogeneous traps
    if (toBePlacedRandomly > 0)
    {
        // Place the seeds
        if (toBePlacedSeeds > 0)
        {
            qDebug("langmuir: placing %d seeds", toBePlacedSeeds);
            int progress = 0;
            while (progress < toBePlacedSeeds)
            {
                int s = m_world.randomNumberGenerator().integer(0,
                            m_world.electronGrid().volume() - 1);
                if ( ! randomIDs.contains(s) && ! traps.contains(s) )
                {
                    randomIDs.push_back(s);
                    randomPotentials.push_back(m_world.parameters().trapPotential);
                    ++progress;
                }
            }
        }

        // Place the grown (if 0, does nothing)
        if (toBePlacedGrown > 0)
        {
            qDebug("langmuir: growing %d traps", toBePlacedGrown);
            int progress = 0;
            while (progress < toBePlacedGrown)
            {
                int trapSeedIndex = m_world.randomNumberGenerator().integer(0,
                                        randomIDs.size() - 1);
                int trapSeedSite = randomIDs.at(trapSeedIndex);
                QVector<int> trapSeedNeighbors = m_world.electronGrid().neighborsSite(
                                        trapSeedSite, 1);

                int newTrapIndex = m_world.randomNumberGenerator().integer(0,
                                        trapSeedNeighbors.size() - 1);
                int newTrapSite = trapSeedNeighbors[newTrapIndex];
                if(m_world.electronGrid().agentType(newTrapSite)!= Agent::Source &&
                   m_world.electronGrid().agentType(newTrapSite)!= Agent::Drain &&
                   m_world.holeGrid().agentType(newTrapSite)!= Agent::Source &&
                   m_world.holeGrid().agentType(newTrapSite)!= Agent::Drain &&
                   !randomIDs.contains(newTrapSite) &&
                   !traps.contains(newTrapSite))
                {
                    randomIDs.push_back(newTrapSite);
                    randomPotentials.push_back(m_world.parameters().trapPotential);
                    ++progress;
                }
            }
        }

        // Apply a deviation to the trap energies (only the randomly generated ones)
        if(abs(m_world.parameters().gaussianStdev) > 0)
        {
            qDebug("langmuir: applying std to randomly placed traps");
            for(int i = 0; i < randomIDs.size(); i++)
            {
                double v = m_world.randomNumberGenerator().normal(0,
                                m_world.parameters().gaussianStdev);
                randomPotentials[i] = randomPotentials[i] + v;
            }
        }

        // Copy traps
        for (int i = 0; i < randomIDs.size(); i++)
        {
            traps.push_back(randomIDs.at(i));
            potentials.push_back(randomPotentials.at(i));
        }
    }

    // Save the trap ids and potentials
    m_world.trapSiteIDs() = traps;
    m_world.trapSitePotentials() = potentials;

    // Now update the grids
    qDebug("langmuir: updating grid with trap info");
    for (int i = 0; i < traps.size(); i++)
    {
        int s = traps.at(i);
        double v = potentials.at(i);
        m_world.electronGrid().addToPotential(s,v);
        m_world.holeGrid().addToPotential(s,v);
    }
}

void Potential::precalculateArrays()
{
    qDebug("langmuir: precalculating interactions");

    int max_x = m_world.parameters().electrostaticCutoff + 1;
    int max_y = m_world.parameters().electrostaticCutoff + 1;
    int max_z = m_world.parameters().electrostaticCutoff + 1;
    int max_h = m_world.parameters().hoppingRange + 1;

    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& R2 = m_world.R2();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    boost::multi_array<double, 3>& sI = m_world.sI();

    R1.resize(boost::extents[max_x][max_y][max_z]);
    R2.resize(boost::extents[max_x][max_y][max_z]);
    iR.resize(boost::extents[max_x][max_y][max_z]);
    eR.resize(boost::extents[max_x][max_y][max_z]);
    sI.resize(boost::extents[max_h][max_h][max_h]);

    // precalculate r1, r2, ri, and set erf to 1.0
    for(int dx = 0; dx < max_x; ++dx)
    {
        for(int dy = 0; dy < max_y; ++dy)
        {
            for(int dz = 0; dz < max_z; ++dz)
            {
                double r2 = dx * dx + dy * dy + dz * dz;
                double r1 = 0.0;
                double ir = 0.0;

                if (r2 > 0)
                {
                    r1 = sqrt(r2);
                    ir = 1.0 / r1;
                }

                R2[dx][dy][dz] =  r2;
                R1[dx][dy][dz] =  r1;
                iR[dx][dy][dz] =  ir;
                eR[dx][dy][dz] = 1.0;
            }
        }
    }  

    // pre-calculate erf
    if (m_world.parameters().coulombGaussianSigma > 0.0)
    {
        double factor = 1.0 / (sqrt(2.0) *
                               m_world.parameters().coulombGaussianSigma);

        for(int dx = 0; dx < max_x; ++dx)
        {
            for(int dy = 0; dy < max_y; ++dy)
            {
                for(int dz = 0; dz < max_z; ++dz)
                {
                    eR[dx][dy][dz] = erf(factor * R1[dx][dy][dz]);
                }
            }
        }
    }

    // pre-calculate self interactions
    double prefactor = m_world.parameters().electrostaticPrefactor;

    for (int dx = 0; dx < max_h; dx++)
    {
        for (int dy = 0; dy < max_h; dy++)
        {
            for (int dz = 0; dz < max_h; dz++)
            {
                // note : eR[dx][dy][dz] = 1.0 if sigma was 0
                sI[dx][dy][dz] = prefactor * iR[dx][dy][dz] * eR[dx][dy][dz];
            }
        }
    }
}

void Potential::updateCouplingConstants()
{
    //These values are used for moving between sites
    boost::multi_array<double, 3>& constants = m_world.couplingConstants();
    int max_h = m_world.parameters().hoppingRange + 1;
    constants.resize(boost::extents[max_h][max_h][max_h]);

    //Assuming coupling(r=1) is 1/3 and coupling(r=2) is 1/27
    double c1 = 1.0 /  3.0;
    double c2 = 1.0 / 27.0;
    for (int dx = 0; dx < max_h; dx++)
    {
        for (int dy = 0; dy < max_h; dy++)
        {
            for (int dz = 0; dz < max_h; dz++)
            {
                double r = sqrt(dx*dx + dy*dy + dz*dz);
                double c = c1 * pow(3.0, r - 1) * pow(c2, r - 1);
                constants[dx][dy][dz] = c;
            }
        }
    }
    constants[0][0][0] = 0;
}

double Potential::coulombE(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    //boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.electrons();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge.charge());
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::coulombImageE(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    //boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.electrons();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge.charge());
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussE(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.electrons();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge.charge() * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussImageE(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.electrons();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge.charge() * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::coulombH(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    //boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.holes();
    Grid &grid = m_world.holeGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge.charge());
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::coulombImageH(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    //boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.holes();
    Grid &grid = m_world.holeGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge.charge());
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussH(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.holes();
    Grid &grid = m_world.holeGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge.charge() * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussImageH(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    QList<ChargeAgent*>& charges = m_world.holes();
    Grid &grid = m_world.holeGrid();

    double potential = 0.0;

    for (int i = 0; i < charges.size(); i++)
    {
        //Q_CHECK_PTR(charges[i]);
        ChargeAgent& charge = *charges[i];
        int site_j = charge.getCurrentSite();

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dy < cutoff) && (dz < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge.charge() * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::coulombD(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    qint32 charge = m_world.parameters().defectsCharge;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    if (charge == 0)
    {
        return potential;
    }

    for (int i = 0; i < m_world.defectSiteIDs().size(); i++)
    {
        int site_j = m_world.defectSiteIDs()[i];

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dz < cutoff) && (dy < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::coulombImageD(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    qint32 charge = m_world.parameters().defectsCharge;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    Grid &grid = m_world.electronGrid();

    double potential = 0.0;

    if (charge == 0)
    {
        return potential;
    }

    for (int i = 0; i < m_world.defectSiteIDs().size(); i++)
    {
        int site_j = m_world.defectSiteIDs()[i];

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dz < cutoff) && (dy < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussD(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    qint32 charge = m_world.parameters().defectsCharge;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    Grid &grid = m_world.electronGrid();
    double potential = 0.0;

    if (charge == 0)
    {
        return potential;
    }

    for (int i = 0; i < m_world.defectSiteIDs().size(); i++)
    {
        int site_j = m_world.defectSiteIDs()[i];

        int dx = grid.xDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dz < cutoff) && (dy < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential += (iR[dx][dy][dz] * charge * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

double Potential::gaussImageD(int site_i)
{
    qint32 cutoff = m_world.parameters().electrostaticCutoff;
    qint32 charge = m_world.parameters().defectsCharge;
    boost::multi_array<double, 3>& R1 = m_world.R1();
    boost::multi_array<double, 3>& iR = m_world.iR();
    boost::multi_array<double, 3>& eR = m_world.eR();
    Grid &grid = m_world.electronGrid();
    double potential = 0.0;

    if (charge == 0)
    {
        return potential;
    }

    for (int i = 0; i < m_world.defectSiteIDs().size(); i++)
    {
        int site_j = m_world.defectSiteIDs()[i];

        int dx = grid.xImageDistancei(site_i, site_j);
        int dy = grid.yDistancei(site_i, site_j);
        int dz = grid.zDistancei(site_i, site_j);

        if ((dx < cutoff) && (dz < cutoff) && (dy < cutoff))
        {
            if (R1[dx][dy][dz] < cutoff)
            {
                potential -= (iR[dx][dy][dz] * charge * eR[dx][dy][dz]);
            }
        }
    }

    return (potential * m_world.parameters().electrostaticPrefactor);
}

}
