#ifndef ARCHIVASAPPLICATION_H
#define ARCHIVASAPPLICATION_H

#include <QApplication>
#include <QStyleFactory>

class ArchivasApplication : public QApplication
{
    Q_OBJECT

public:
    ArchivasApplication(int& argc, char** argv);
    ~ArchivasApplication();

    void setupStyle();
    void setupIcon();
};

#endif // ARCHIVASAPPLICATION_H

