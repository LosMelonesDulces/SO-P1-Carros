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

    // Cargar pixmaps (asegúrate de que existan estos ficheros)
    carPixmapNormal.load("car.png");
    carPixmapSport.load("car2.png");
    carPixmapEmergency.load("car3.png");

    // Repaint ~60 FPS
    startTimer(16);

    // Señales de red
    connect(tcpSocket, &QTcpSocket::readyRead, this, &SquareWidget::readServerData);
    connect(tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &SquareWidget::handleSocketError);
    connect(reconnectTimer, &QTimer::timeout, this, &SquareWidget::tryReconnect);

    tcpSocket->connectToHost("127.0.0.1", 8080);
    reconnectTimer->start(5000);
}

void SquareWidget::handleSocketError(QAbstractSocket::SocketError /*error*/) {
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

        // Contar comas de nivel 1 para distinguir inicial vs acción
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
    QStringList parts;
    QString token;
    int level = 0;
    for (QChar c : msg) {
        if (c == '(') { ++level; if (level == 1) continue; }
        if (c == ')') { --level; if (level == 0) break; }
        if (c == ',' && level == 1) {
            parts << token; token.clear();
        } else {
            token += c;
        }
    }
    parts << token;

    // Extraer valores
    // int leftCount = parts[0].toInt(); // opcional usar
    // int rightCount = parts[1].toInt();
    QStringList tLeft   = parts[2].mid(1, parts[2].length()-2).split(',', Qt::SkipEmptyParts);
    QStringList tRight  = parts[3].mid(1, parts[3].length()-2).split(',', Qt::SkipEmptyParts);
    QStringList idLeft  = parts[4].mid(1, parts[4].length()-2).split(',', Qt::SkipEmptyParts);
    QStringList idRight = parts[5].mid(1, parts[5].length()-2).split(',', Qt::SkipEmptyParts);
    roundRobinBit = parts[6].toInt();

    if (roundRobinBit == 1) {
        qDebug() << "es round robin";
    }

    leftQueue.clear(); rightQueue.clear();
    leftIds.clear();   rightIds.clear();
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

    int side  = f[0].toInt();    // 0=izq,1=der
    int carId = f[1].toInt();
    // int crossTime = f[2].toInt();

    QVector<int>     &ids = (side == 0 ? leftIds  : rightIds);
    QVector<CarType> &q   = (side == 0 ? leftQueue: rightQueue);

    int idx = ids.indexOf(carId);
    qDebug() << "[DEBUG] Acción recibida: side=" << side
             << " carId=" << carId
             << " idxEnCola=" << idx;
    if (idx < 0) return;

    CarType type = q.at(idx);
    qDebug() << "[DEBUG] Tipo de coche extraído:" << type;

    if (side == 0) {
        startCarFromLeft(type);
    } else {
        startCarFromRight(type);
    }

    // Elimino de la cola
    q.removeAt(idx);
    ids.removeAt(idx);

    // Fuerzo repintado inmediato
    update();
}


void SquareWidget::startCarFromLeft(CarType type) {
    cars.append({300, 300, type, true});
    isCrossing = true;
}

void SquareWidget::startCarFromRight(CarType type) {
    cars.append({900, 300, type, false});
    isCrossing = true;
}

void SquareWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    // Dibujo de caminos
    painter.setPen(QPen(Qt::black,4));
    painter.drawLine(300,200,900,200);
    painter.drawLine(300,400,900,400);

    // Dibujo de autos en cruce
    for (auto &c : cars) {
        QPixmap pm = (c.type==Sport?carPixmapSport:
                      c.type==Emergency?carPixmapEmergency:carPixmapNormal);
        if (!c.movingRight)
            pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(c.xPos, c.yPos,50,50,pm);
    }

    // Colas izquierda
    for (int i=0; i<leftQueue.size(); ++i) {
        QPixmap pm = (leftQueue[i]==Sport?carPixmapSport:
                      leftQueue[i]==Emergency?carPixmapEmergency:carPixmapNormal);
        painter.drawPixmap(150,300 - i*55,50,50,pm);
    }
    // Colas derecha
    for (int i=0; i<rightQueue.size(); ++i) {
        QPixmap pm = (rightQueue[i]==Sport?carPixmapSport:
                      rightQueue[i]==Emergency?carPixmapEmergency:carPixmapNormal);
        pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(1050,300 - i*55,50,50,pm);
    }

    // Indicador de tipo seleccionado
    painter.setFont(QFont("Arial",12));
    painter.setPen(Qt::black);
    painter.drawText(50,50,"Tipo seleccionado:");
    QPixmap sel = (selectedType==Sport?carPixmapSport:
                   selectedType==Emergency?carPixmapEmergency:carPixmapNormal);
    painter.drawPixmap(200,25,50,50,sel);
    painter.drawText(260,50,
        selectedType==Sport?"Sport":
        selectedType==Emergency?"Emergency":"Normal"
    );
}

void SquareWidget::keyPressEvent(QKeyEvent *e) {
    switch (e->key()) {
        case Qt::Key_N: selectedType = Normal;    break;
        case Qt::Key_S: selectedType = Sport;     break;
        case Qt::Key_E: selectedType = Emergency; break;
        // Ya no manejamos A/D/Space/X si todo viene del servidor
        default: QWidget::keyPressEvent(e);
    }
    update();
}

void SquareWidget::timerEvent(QTimerEvent *) {
    for (int i = cars.size()-1; i>=0; --i) {
        auto &c = cars[i];
        int speed = (c.type==Sport?10:5);
        c.xPos += c.movingRight? speed : -speed;
        if ((c.movingRight && c.xPos>=900) ||
            (!c.movingRight && c.xPos<=300)) {
            cars.removeAt(i);
            isCrossing = false;
        }
    }
    update();
}
