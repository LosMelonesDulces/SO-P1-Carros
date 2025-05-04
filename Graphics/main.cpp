#include <QApplication>
#include "SquareWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    SquareWidget w;
    w.resize(400, 300);
    w.show();

    return app.exec();
}
