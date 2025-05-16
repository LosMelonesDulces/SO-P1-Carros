// SquareWidget.cpp
#include "SquareWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QFont>
#include <QDebug>

SquareWidget::SquareWidget(QWidget *parent)
    : QWidget(parent),
      tcpSocket(new QTcpSocket(this)),
      reconnectTimer(new QTimer(this))
{
    setFocusPolicy(Qt::StrongFocus);

    // Load pixmaps
    carPixmapNormal.load("car.png");
    carPixmapSport.load("car2.png");
    carPixmapEmergency.load("car3.png");

    // 60 FPS repaint
    startTimer(16);

    // Network signals
    connect(tcpSocket, &QTcpSocket::readyRead, this, &SquareWidget::readServerData);
    connect(tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &SquareWidget::handleSocketError);
    connect(reconnectTimer, &QTimer::timeout, this, &SquareWidget::tryReconnect);

    tcpSocket->connectToHost("127.0.0.1", 8080);
    reconnectTimer->start(5000);
}

void SquareWidget::handleSocketError(QAbstractSocket::SocketError /*err*/) {
    qWarning() << "Error de conexión:" << tcpSocket->errorString();
    reconnectTimer->start(5000);
}

void SquareWidget::tryReconnect() {
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Intentando reconexión...";
        tcpSocket->abort();
        tcpSocket->connectToHost("127.0.0.1", 8080);
    }
}

void SquareWidget::readServerData() {
    while (tcpSocket->canReadLine()) {
        QString message = tcpSocket->readLine().trimmed();
        qDebug() << "Mensaje recibido:" << message;

        // Count top-level commas
        int level = 0, commas = 0;
        for (QChar c : message) {
            if (c == '(') ++level;
            else if (c == ')') --level;
            else if (c == ',' && level == 1) ++commas;
        }
        if (commas > 4)
            parseInitialConfig(message);
        else
            parseActionMessage(message);
    }
}

void SquareWidget::parseInitialConfig(const QString &msg) {
    // Helper to clean and split a parenthesized list
    auto extract = [&](const QString &s) {
        QString tmp = s;
        tmp.remove('(').remove(')');
        tmp = tmp.trimmed();
        return tmp.split(',', Qt::SkipEmptyParts);
    };

    // Split at commas at level 1
    QStringList parts;
    QString token;
    int level = 0;
    for (QChar c : msg) {
        if (c == '(') { ++level; if (level == 1) continue; }
        if (c == ')') { --level; if (level == 0) break; }
        if (c == ',' && level == 1) {
            parts << token;
            token.clear();
        } else {
            token += c;
        }
    }
    parts << token;

    // Extract
    QStringList tLeft   = extract(parts[2]);
    QStringList tRight  = extract(parts[3]);
    QStringList idLeft  = extract(parts[4]);
    QStringList idRight = extract(parts[5]);
    roundRobinBit       = parts[6].toInt();

    if (roundRobinBit == 1) qDebug() << "es round robin";

    // Clear old
    cars.clear();
    leftQueue.clear();  rightQueue.clear();
    leftIds.clear();    rightIds.clear();

    // Populate
    for (auto &s : tLeft)   leftQueue.append(static_cast<CarType>(s.toInt()));
    for (auto &s : tRight)  rightQueue.append(static_cast<CarType>(s.toInt()));
    for (auto &s : idLeft)  leftIds.append(s.toInt());
    for (auto &s : idRight) rightIds.append(s.toInt());

    update();
}

void SquareWidget::parseActionMessage(const QString &msg) {
    QString clean = msg;
    clean.remove('(').remove(')');
    QStringList f = clean.split(',', Qt::SkipEmptyParts);
    if (f.size() != 3) return;

    int side  = f[0].toInt();
    int carId = f[1].toInt();
    int idx   = (side == 0 ? leftIds.indexOf(carId)
                          : rightIds.indexOf(carId));
    qDebug() << "[DEBUG] Acción:" << side << carId << "idx=" << idx;
    if (idx < 0) return;

    QVector<CarType> &q = (side == 0 ? leftQueue : rightQueue);
    CarType type = q.at(idx);

    if (side == 0) startCarFromLeft(type);
    else          startCarFromRight(type);

    q.removeAt(idx);
    if (side == 0) leftIds.removeAt(idx);
    else           rightIds.removeAt(idx);

    update();
}

void SquareWidget::startCarFromLeft(CarType type) {
    cars.append({300,300,type,true});
    isCrossing = true;
}

void SquareWidget::startCarFromRight(CarType type) {
    cars.append({900,300,type,false});
    isCrossing = true;
}

void SquareWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    painter.setPen(QPen(Qt::black,4));
    painter.drawLine(300,200,900,200);
    painter.drawLine(300,400,900,400);

    for (auto &c : cars) {
        QPixmap pm = (c.type==Sport?carPixmapSport:
                      c.type==Emergency?carPixmapEmergency:carPixmapNormal);
        if (!c.movingRight) pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(c.xPos,c.yPos,50,50,pm);
    }

    for (int i=0; i<leftQueue.size(); ++i) {
        QPixmap pm = (leftQueue[i]==Sport?carPixmapSport:
                      leftQueue[i]==Emergency?carPixmapEmergency:carPixmapNormal);
        painter.drawPixmap(150,300-i*55,50,50,pm);
    }
    for (int i=0; i<rightQueue.size(); ++i) {
        QPixmap pm = (rightQueue[i]==Sport?carPixmapSport:
                      rightQueue[i]==Emergency?carPixmapEmergency:carPixmapNormal);
        pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(1050,300-i*55,50,50,pm);
    }

    painter.setFont(QFont("Arial",12));
    painter.setPen(Qt::black);
    painter.drawText(50,50,"Tipo seleccionado:");
    QPixmap sel = (selectedType==Sport?carPixmapSport:
                   selectedType==Emergency?carPixmapEmergency:carPixmapNormal);
    painter.drawPixmap(200,25,50,50,sel);
    painter.drawText(260,50,
        selectedType==Sport?"Sport":
        selectedType==Emergency?"Emergency":"Normal");
}

void SquareWidget::keyPressEvent(QKeyEvent *e) {
    switch (e->key()) {
        case Qt::Key_N: selectedType = Normal;    break;
        case Qt::Key_S: selectedType = Sport;     break;
        case Qt::Key_E: selectedType = Emergency; break;
        default: QWidget::keyPressEvent(e);
    }
    update();
}

void SquareWidget::timerEvent(QTimerEvent *) {
    for (int i=cars.size()-1; i>=0; --i) {
        auto &c = cars[i];
        int speed = (c.type==Sport?10:5);
        c.xPos += c.movingRight?speed:-speed;
        if ((c.movingRight && c.xPos>=900) ||
            (!c.movingRight && c.xPos<=300)) {
            cars.removeAt(i);
            isCrossing = false;
        }
    }
    update();
}
