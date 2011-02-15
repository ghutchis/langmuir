#ifndef GRID_VIEW_H
#define GRID_VIEW_H

#include <QtCore>
#include <QtGui>
#include <GL/glew.h>
#include <QGLWidget>
#include <QGLShader>
#include <QGLShaderProgram>
#include <QMatrix>
#include <QGLBuffer>

namespace Langmuir
{

  class GridViewGL:public QGLWidget
  {
  Q_OBJECT public:
    GridViewGL (QWidget * parent);
   ~GridViewGL ();
    QSize minimumSizeHint () const;
    QSize sizeHint () const;

  protected:
    void initializeGL ();
    void resizeGL (int w, int h);
    void paintGL ();

  private:

   GLuint program;
   GLuint vshader;
   GLuint fshader;

   GLuint varray;
   GLuint vbuffer;
   GLuint fbuffer;

  };

  class MainWindow:public QWidget
  {
  Q_OBJECT public:
    MainWindow ();

  private:
    GridViewGL * glWidget;
  };

}

#endif
