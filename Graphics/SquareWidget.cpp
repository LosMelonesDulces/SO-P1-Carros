// SquareWidget.cpp
#include "SquareWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QFont>
#include <QDebug>

static const int startX       = 300;
static const int endX         = 900;
static const int fixedY       = 300;
static const double timerMs   = 16.0;      // Intervalo de QTimer en milisegundos
static const double fps       = 1000.0 / timerMs; // ≈ 62.5 frames por segundo

SquareWidget::SquareWidget(QWidget *parent)
    : QWidget(parent),
      tcpSocket(new QTcpSocket(this)),
      reconnectTimer(new QTimer(this))
{
    setFocusPolicy(Qt::StrongFocus);

    // Cargar los iconos
    carPixmapNormal.load("car.png");
    carPixmapSport.load("car2.png");
    carPixmapEmergency.load("car3.png");

    // Lanzar repaint cada ~16 ms (≈60 FPS)
    startTimer(int(timerMs));

    // Conexiones de red
    connect(tcpSocket, &QTcpSocket::readyRead, this, &SquareWidget::readServerData);
    connect(tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &SquareWidget::handleSocketError);
    connect(reconnectTimer, &QTimer::timeout, this, &SquareWidget::tryReconnect);

    tcpSocket->connectToHost("127.0.0.1", 8080);
    reconnectTimer->start(5000);
}

void SquareWidget::handleSocketError(QAbstractSocket::SocketError)
{
    qWarning() << "Error de conexión:" << tcpSocket->errorString();
    reconnectTimer->start(5000);
}

void SquareWidget::tryReconnect()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Intentando reconexión...";
        tcpSocket->abort();
        tcpSocket->connectToHost("127.0.0.1", 8080);
    }
}

void SquareWidget::readServerData()
{
    while (tcpSocket->canReadLine()) {
        QString message = tcpSocket->readLine().trimmed();
        qDebug() << "Mensaje recibido:" << message;

        // Contar comas de nivel 1
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

void SquareWidget::parseInitialConfig(const QString &msg)
{
    // Helper para limpiar "(...)" y dividir por comas
    auto extract = [&](const QString &s){
        QString tmp = s;
        tmp.remove('(').remove(')');
        tmp = tmp.trimmed();
        return tmp.split(',', Qt::SkipEmptyParts);
    };

    // Particionar el mensaje en 7 partes por comas de nivel 1
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

    // Extraer listas
    QStringList tLeft   = extract(parts[2]);
    QStringList tRight  = extract(parts[3]);
    QStringList idLeft  = extract(parts[4]);
    QStringList idRight = extract(parts[5]);
    roundRobinBit       = parts[6].toInt();

    if (roundRobinBit == 1)
        qDebug() << "es round robin";

    // Reset estado
    cars.clear();
    leftQueue.clear();  rightQueue.clear();
    leftIds.clear();    rightIds.clear();

    // Poblar
    for (auto &s : tLeft)   leftQueue.append(static_cast<CarType>(s.toInt()));
    for (auto &s : tRight)  rightQueue.append(static_cast<CarType>(s.toInt()));
    for (auto &s : idLeft)  leftIds.append(s.toInt());
    for (auto &s : idRight) rightIds.append(s.toInt());

    update();
}

void SquareWidget::parseActionMessage(const QString &msg)
{
    QString clean = msg;
    clean.remove('(').remove(')');
    QStringList f = clean.split(',', Qt::SkipEmptyParts);
    if (f.size() != 3) return;

    int side        = f[0].toInt();      // 0=izq,1=der
    int carId       = f[1].toInt();
    double seconds  = f[2].toDouble();   // ahora interpretado como segundos

    QVector<int> &ids = (side == 0 ? leftIds : rightIds);
    int idx = ids.indexOf(carId);
    if (idx < 0) return;

    CarType type = (side == 0 ? leftQueue[idx] : rightQueue[idx]);
    if (side == 0)
        startCarFromLeft(type, seconds);
    else
        startCarFromRight(type, seconds);

    // Quitar de la cola
    if (side == 0) {
        leftQueue.removeAt(idx);
        leftIds.removeAt(idx);
    } else {
        rightQueue.removeAt(idx);
        rightIds.removeAt(idx);
    }

    update();
}

void SquareWidget::startCarFromLeft(CarType type, double secondsToCross)
{
    double dist = endX - startX;                      // píxeles totales
    double frames = secondsToCross * fps;             // frames esperados
    double speed = (frames > 0 ? dist / frames : dist);
    cars.append({ double(startX), double(fixedY), type, true, secondsToCross, speed });
    isCrossing = true;
}

void SquareWidget::startCarFromRight(CarType type, double secondsToCross)
{
    double dist = endX - startX;
    double frames = secondsToCross * fps;
    double speed = (frames > 0 ? dist / frames : dist);
    cars.append({ double(endX), double(fixedY), type, false, secondsToCross, speed });
    isCrossing = true;
}

void SquareWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    // Dibujar carriles
    painter.setPen(QPen(Qt::black,4));
    painter.drawLine(startX,200,endX,200);
    painter.drawLine(startX,400,endX,400);

    // Dibujar coches en cruce
    for (auto &c : cars) {
        QPixmap pm = (c.type==Sport ? carPixmapSport
                    : c.type==Emergency ? carPixmapEmergency
                                         : carPixmapNormal);
        if (!c.movingRight)
            pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(int(c.xPos), int(c.yPos), 50, 50, pm);
    }

    // Colas izquierda
    for (int i=0; i<leftQueue.size(); ++i) {
        QPixmap pm = (leftQueue[i]==Sport ? carPixmapSport
                    : leftQueue[i]==Emergency ? carPixmapEmergency
                                              : carPixmapNormal);
        painter.drawPixmap(150,300 - i*55,50,50,pm);
    }

    // Colas derecha
    for (int i=0; i<rightQueue.size(); ++i) {
        QPixmap pm = (rightQueue[i]==Sport ? carPixmapSport
                    : rightQueue[i]==Emergency ? carPixmapEmergency
                                              : carPixmapNormal);
        pm = pm.transformed(QTransform().scale(-1,1));
        painter.drawPixmap(1050,300 - i*55,50,50,pm);
    }

    // Indicador de tipo seleccionado
    painter.setFont(QFont("Arial",12));
    painter.setPen(Qt::black);
    painter.drawText(50,50,"Tipo seleccionado:");
    QPixmap sel = (selectedType==Sport ? carPixmapSport
                : selectedType==Emergency ? carPixmapEmergency
                                          : carPixmapNormal);
    painter.drawPixmap(200,25,50,50,sel);
    painter.drawText(260,50,
        selectedType==Sport ? "Sport"
      : selectedType==Emergency ? "Emergency"
                                 : "Normal");
}

void SquareWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
        case Qt::Key_N: selectedType = Normal;    break;
        case Qt::Key_S: selectedType = Sport;     break;
        case Qt::Key_E: selectedType = Emergency; break;
        default: QWidget::keyPressEvent(e);
    }
    update();
}

void SquareWidget::timerEvent(QTimerEvent *)
{
    for (int i = cars.size()-1; i >= 0; --i) {
        auto &c = cars[i];
        c.xPos += c.movingRight ? c.vx : -c.vx;
        if ((c.movingRight && c.xPos >= endX) ||
            (!c.movingRight && c.xPos <= startX)) {
            cars.removeAt(i);
            isCrossing = false;
        }
    }
    update();
}
