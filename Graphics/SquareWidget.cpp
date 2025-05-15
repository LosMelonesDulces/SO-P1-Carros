#include "SquareWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QFont>
#include <QTcpSocket>    // Nueva inclusión
#include <QTimer>        // Nueva inclusión
#include <QDebug>        // Para qDebug()
#include <QAbstractSocket> // Para manejo de errores

SquareWidget::SquareWidget(QWidget *parent)
    : QWidget(parent),
      tcpSocket(new QTcpSocket(this)),      // Inicializar socket
      reconnectTimer(new QTimer(this))      // Inicializar timer
{
    setFocusPolicy(Qt::StrongFocus);

    // Cargar imágenes de autos
    carPixmapNormal.load("car.png");
    carPixmapSport.load("car2.png");
    carPixmapEmergency.load("car3.png");

    if (carPixmapNormal.isNull() || carPixmapSport.isNull() || carPixmapEmergency.isNull()) {
        qWarning("Uno o más iconos no se pudieron cargar.");
    }

    // Configurar temporizador de animación
    startTimer(16); // Aproximadamente 60 FPS

    // Configurar conexiones de red
    connect(tcpSocket, &QTcpSocket::readyRead, this, &SquareWidget::readServerData);
    connect(tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &SquareWidget::handleSocketError);
    connect(reconnectTimer, &QTimer::timeout, this, &SquareWidget::tryReconnect);

    // Conectar al servidor (ajustar puerto según configuración del servidor)
    tcpSocket->connectToHost("127.0.0.1", 8080);
    reconnectTimer->start(5000); // Intentar reconexión cada 5 segundos
}

void SquareWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    // Dibujar el camino
    painter.setPen(QPen(Qt::black, 4));
    painter.drawLine(300, 200, 900, 200);
    painter.drawLine(300, 400, 900, 400);

    // Dibujar autos en movimiento
    for (const Car &car : cars) {
        QPixmap pixmap;
        switch (car.type) {
            case Normal: pixmap = carPixmapNormal; break;
            case Sport: pixmap = carPixmapSport; break;
            case Emergency: pixmap = carPixmapEmergency; break;
        }
        if (!car.movingRight) {
            pixmap = pixmap.transformed(QTransform().scale(-1, 1));
        }
        painter.drawPixmap(car.xPos, car.yPos, 50, 50, pixmap);
    }

    // Dibujar colas
    for (int i = 0; i < (int)leftQueue.size(); ++i) {
        QPixmap pixmap;
        switch (leftQueue[i]) {
            case Normal: pixmap = carPixmapNormal; break;
            case Sport: pixmap = carPixmapSport; break;
            case Emergency: pixmap = carPixmapEmergency; break;
        }
        painter.drawPixmap(150, 300 - i * 55, 50, 50, pixmap);
    }

    for (int i = 0; i < (int)rightQueue.size(); ++i) {
        QPixmap pixmap;
        switch (rightQueue[i]) {
            case Normal: pixmap = carPixmapNormal; break;
            case Sport: pixmap = carPixmapSport; break;
            case Emergency: pixmap = carPixmapEmergency; break;
        }
        QPixmap mirrored = pixmap.transformed(QTransform().scale(-1, 1));
        painter.drawPixmap(1050, 300 - i * 55, 50, 50, mirrored);
    }

    // Mostrar tipo seleccionado
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(50, 50, "Tipo seleccionado:");

    QPixmap selectedPixmap;
    QString label;
    switch (selectedType) {
        case Normal:
            selectedPixmap = carPixmapNormal;
            label = "Normal";
            break;
        case Sport:
            selectedPixmap = carPixmapSport;
            label = "Sport";
            break;
        case Emergency:
            selectedPixmap = carPixmapEmergency;
            label = "Emergency";
            break;
    }

    painter.drawPixmap(200, 25, 50, 50, selectedPixmap);
    painter.drawText(260, 50, label);
}

void SquareWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_N:
            selectedType = Normal;
            break;
        case Qt::Key_S:
            selectedType = Sport;
            break;
        case Qt::Key_E:
            selectedType = Emergency;
            break;
        case Qt::Key_A:
            if (leftQueue.size() < 10) leftQueue.push_back(selectedType);
            break;
        case Qt::Key_D:
            if (rightQueue.size() < 10) rightQueue.push_back(selectedType);
            break;
        case Qt::Key_Space:
            if (!leftQueue.empty() && !isCrossing) {
                startCarFromLeft();
                leftQueue.erase(leftQueue.begin());
            }
            break;
        case Qt::Key_X:
            if (!rightQueue.empty() && !isCrossing) {
                startCarFromRight();
                rightQueue.erase(rightQueue.begin());
            }
            break;
    }

    update();
}

void SquareWidget::timerEvent(QTimerEvent * /* event */)
{
    for (auto it = cars.begin(); it != cars.end(); ) {
        int speed = (it->type == Sport) ? 10 : 5;

        if (it->movingRight) {
            it->xPos += speed;
        } else {
            it->xPos -= speed;
        }

        if ((it->movingRight && it->xPos >= 900) ||
            (!it->movingRight && it->xPos <= 300)) {
            it = cars.erase(it);
            isCrossing = false;
            } else {
                ++it;
            }
    }

    update();
}

void SquareWidget::startCarFromLeft()
{
    Car car = {300, 300, leftQueue.front(), true};
    cars.push_back(car);
    isCrossing = true;
}

void SquareWidget::startCarFromRight()
{
    Car car = {900, 300, rightQueue.front(), false};
    cars.push_back(car);
    isCrossing = true;
}

// Nuevos métodos de manejo de red
void SquareWidget::readServerData() {
    qDebug() << "Bytes disponibles:" << tcpSocket->bytesAvailable();

    while (tcpSocket->canReadLine()) {
        QString message = tcpSocket->readLine().trimmed();
        qDebug() << "Mensaje recibido:" << message;
    }

    qDebug() << "Fin de readServerData";
}

void SquareWidget::handleSocketError(QAbstractSocket::SocketError error) {
    qWarning() << "Error de conexión:" << tcpSocket->errorString();
    reconnectTimer->start();
}

void SquareWidget::tryReconnect() {
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Intentando reconexión...";
        tcpSocket->abort();
        tcpSocket->connectToHost("127.0.0.1", 8080);
    }
}
