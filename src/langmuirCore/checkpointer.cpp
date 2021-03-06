#include "checkpointer.h"
#include "chargeagent.h"
#include "output.h"
#include "world.h"
#include "rand.h"
#include "keyvalueparser.h"
#include "fluxagent.h"
#include "gzipper.h"

#include <fstream>
#include <limits>
#include <iomanip>

namespace Langmuir
{

CheckPointer::CheckPointer(World &world, QObject *parent) :
    QObject(parent), m_world(world)
{
}

void CheckPointer::load(const QString &fileName, ConfigurationInfo &configInfo)
{
    // Unzip the input file
    bool wasZipped = false;
    QString unzipped = gunzip(fileName, &wasZipped);

    // Open the stream
    std::ifstream stream;
    stream.open(unzipped.toLatin1().constData());

    if (!stream)
    {
        qFatal("langmuir: error opening file: %s",qPrintable(fileName));
    }

    // Get the QMetaEnum object to map strings to the correct enum
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));

    // We need to be careful with the random number generator
    bool readRandomState = false;

    qDebug("langmuir: reading input file");
    while (!stream.eof())
    {
        // Expecting a section header
        QString section;
        stream >> section;

        // If no section header, we are done
        if (stream.eof()) break;

        // Check for an error
        checkStream(stream, QString("expected a section header or empty line"));

        if (!section.isEmpty())
        {
            // Examine the section header
            section.replace(QRegExp("[\\[\\]\\s]"),"");
            int eSection = QME.keyToValue(section.toLatin1().constData());
            if (eSection < 0)
            {
                qDebug("langmuir: invalid section encountered: %s", qPrintable(section));
                qDebug("langmuir: valid sections are:");
                for (int i = 0; i < QME.keyCount(); i++)
                {
                    qDebug("langmuir: [%s]",QME.key(i));
                }
                qFatal("langmuir: exiting");
            }

            // Load according to the section encountered
            switch (eSection)
            {
                case Parameters:
                {
                    // the parameter case terminates at the end of the file
                    // or if the word end is encountered
                    loadParameters(stream);
                    break;
                }

                case Electrons:
                {
                    loadElectrons(stream, configInfo);
                    break;
                }

                case Holes:
                {
                    loadHoles(stream, configInfo);
                    break;
                }

                case Defects:
                {
                    loadDefects(stream, configInfo);
                    break;
                }

                case Traps:
                {
                    loadTraps(stream, configInfo);
                    break;
                }

                case TrapPotentials:
                {
                    loadTrapPotentials(stream, configInfo);
                    break;
                }

                case RandomState:
                {
                    loadRandomState(stream);
                    readRandomState = true;
                    break;
                }

                case FluxState:
                {
                    loadFluxState(stream, configInfo);
                    break;
                }

                default:
                {
                    qDebug("invalid section encountered: %s", qPrintable(section));
                    qDebug("\tvalid sections are:");
                    for (int i = 0; i < QME.keyCount(); i++)
                    {
                        qDebug("\t\t[%s]",QME.key(i));
                    }
                    qFatal("langmuir: exiting");
                    break;
                }
            }
        }
    }

    // Seed the random number generator correctly
    if (m_world.parameters().randomSeed > 0)
    {
        if (readRandomState)
        {
            qDebug("langmuir: ignoring random.seed = %d",
                     (unsigned int)m_world.parameters().randomSeed);
        }
        else
        {
            qDebug("langmuir: seeding random number generator with random.seed = %d",
                     (unsigned int)m_world.parameters().randomSeed);
            m_world.randomNumberGenerator().seed(m_world.parameters().randomSeed);
        }
    }

    // zip the input file
    if (wasZipped)
    {
        gzip(unzipped);
    }
}

void CheckPointer::save(const QString& fileName)
{
    //qDebug("langmuir: saving checkpoint file: %d", m_world.parameters().currentStep);
    OutputInfo info(fileName,&m_world.parameters());

    std::ofstream stream;
    stream.open(info.absoluteFilePath().toLatin1().constData());

    if (!stream)
    {
        qFatal("langmuir: error opening file: %s",qPrintable(fileName));
    }

    saveElectrons(stream)      << '\n';
    saveHoles(stream)          << '\n';
    saveDefects(stream)        << '\n';
    saveTraps(stream)          << '\n';

    if (m_world.parameters().outputChkTrapPotential)
    {
        saveTrapPotentials(stream) << '\n';
    }

    saveFluxState(stream)      << '\n';
    saveRandomState(stream)    << '\n';
    saveParameters(stream);

    stream.flush();
}

void CheckPointer::checkStream(std::istream& stream, const QString& message)
{
    if (!stream.good())
    {
        if (stream.eof())
        {
            QString error = "stream error: std::ifstream.eof() == true";
            if (!message.isEmpty()) { error += QString("\n\t%1").arg(message); }
            qFatal("langmuir: %s",qPrintable(error));
        }
        if (stream.bad())
        {
            QString error = "stream error: std::ifstream.bad() == true";
            if (!message.isEmpty()) { error += QString("\n\t%1").arg(message); }
            qFatal("langmuir: %s",qPrintable(error));
        }
        if (stream.fail())
        {
            QString error = "stream error: std::ifstream.fail() == true";
            if (!message.isEmpty()) { error += QString("\n\t%1").arg(message); }
            qFatal("langmuir: %s",qPrintable(error));
        }
        QString error = "stream error: std::ifstream.good() == false";
        if (!message.isEmpty()) { error += QString("\n\t%1").arg(message); }
        qFatal("langmuir: %s",qPrintable(error));
    }
}

std::istream& CheckPointer::loadElectrons(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.electrons.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of electrons"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of electrons", qPrintable(token));
    }

    // Read the values
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected electron %1 of %2").arg(i+1).arg(size));
        unsigned int value = token.toUInt(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
                   "expected electron %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.electrons.push_back(value);
    }

    // Return the stream
    return stream;
}

std::istream& CheckPointer::loadHoles(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.holes.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of holes"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of holes", qPrintable(token));
    }

    // Read the values
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected hole %1 of %2").arg(i+1).arg(size));
        unsigned int value = token.toUInt(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
                   "expected hole %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.holes.push_back(value);
    }

    // Return the stream
    return stream;
}

std::istream& CheckPointer::loadDefects(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.defects.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of defects"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of defects", qPrintable(token));
    }

    // Read the values
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected defect %1 of %2").arg(i+1).arg(size));
        unsigned int value = token.toUInt(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
                   "expected defect %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.defects.push_back(value);
    }

    // Return the stream
    return stream;
}

std::istream& CheckPointer::loadTraps(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.traps.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of traps"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of traps", qPrintable(token));
    }

    // Read the values (sites)
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected trap %1 of %2").arg(i+1).arg(size));
        unsigned int value = token.toUInt(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
                   "expected trap %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.traps.push_back(value);
    }

    // Return the stream
    return stream;
}

std::istream& CheckPointer::loadTrapPotentials(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.trapPotentials.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of trap potentials"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of trap potentials", qPrintable(token));
    }

    // Read the values (sites)
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected trap potential %1 of %2").arg(i+1).arg(size));
        double value = token.toDouble(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to double\n\t"
                   "expected trap potential %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.trapPotentials.push_back(value);
    }

    // Return the stream
    return stream;
}

std::istream& CheckPointer::loadParameters(std::istream &stream)
{
    while (!stream.eof())
    {
        std::string string;
        std::getline(stream, string);

        // End of file reached
        if (stream.eof())
        {
            return stream;
        }

        // Check for errors
        checkStream(stream, QString("expected blank line, key=value pair, end, or eof"));

        // Convert to QString
        QString line = QString::fromStdString(string).trimmed();

        // Make sure we are not at the end of the parameter section
        if (line.toLower() == "end") return stream;

        // Attempt to parse the line as a key=value pair
        m_world.keyValueParser().parse(line);
    }
    return stream;
}

std::istream& CheckPointer::loadRandomState(std::istream &stream)
{
    stream >> m_world.randomNumberGenerator();
    checkStream(stream, QString("expected random number generator state"));
    qDebug("langmuir: loaded a random number generator state");
    return stream;
}

std::istream& CheckPointer::loadFluxState(std::istream &stream, ConfigurationInfo &configInfo)
{
    QString token =    "";
    bool       ok = false;

    // Clear the old sites
    configInfo.fluxInfo.clear();

    // Get the number to values to read
    stream >> token;
    checkStream(stream, QString("expected number of fluxes * 2"));
    unsigned int size = token.toUInt(&ok);
    if (!ok)
    {
        qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
               "expected number of fluxes * 2", qPrintable(token));
    }

    // Read the values (sites)
    for (unsigned int i = 0; i < size; i++)
    {
        stream >> token;
        checkStream(stream, QString("expected flux %1 of %2").arg(i+1).arg(size));
        quint64 value = token.toULongLong(&ok);
        if (!ok)
        {
            qFatal("langmuir: stream error: can not convert %s to unsigned int\n\t"
                   "expected flux %d of %d", qPrintable(token),i+1,size);
        }
        configInfo.fluxInfo.push_back(value);
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveElectrons(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(Electrons);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.electrons().size();
    foreach(ChargeAgent* charge, m_world.electrons())
    {
        stream << '\n' << charge->getCurrentSite();
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveHoles(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(Holes);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.holes().size();
    foreach(ChargeAgent* charge, m_world.holes())
    {
        stream << '\n' << charge->getCurrentSite();
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveDefects(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(Defects);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.defectSiteIDs().size();
    foreach(int site, m_world.defectSiteIDs())
    {
        stream << '\n' << site;
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveTraps(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(Traps);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.trapSiteIDs().size();
    foreach(int site, m_world.trapSiteIDs())
    {
        stream << '\n' << site;
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveTrapPotentials(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(TrapPotentials);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.trapSitePotentials().size();

    int precision = std::numeric_limits<double>::digits10+2;
    stream << std::scientific;
    foreach(double value, m_world.trapSitePotentials())
    {
        stream << '\n' << std::setprecision(precision) << value;
    }

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveParameters(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(Parameters);

    // Output info
    stream << '[' << name << ']';
    stream << m_world.keyValueParser();

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveRandomState(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(RandomState);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.randomNumberGenerator();

    // Return the stream
    return stream;
}

std::ostream& CheckPointer::saveFluxState(std::ostream &stream)
{
    // Get the section name
    const QMetaObject &QMO = CheckPointer::staticMetaObject;
    QMetaEnum QME = QMO.enumerator(QMO.indexOfEnumerator("Section"));
    QString name = QME.key(FluxState);

    // Output info
    stream << '[' << name << ']';
    stream << '\n' << m_world.fluxes().size() * 2;
    foreach (FluxAgent *flux, m_world.fluxes())
    {
        stream << '\n' << flux->attempts();
        stream << '\n' << flux->successes();
    }

    // Return the stream
    return stream;
}

}
