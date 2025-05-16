// SquareWidget.cpp
#include "SquareWidget.h"
#include <QPainter>
#include <QKeyEvent>
#include <QTimerEvent>
#include <QFont>
#include <QDebug>

static const int   startX    = 300;
static const int   endX      = 900;
static const int   fixedY    = 300;
static const double timerMs  = 16.0;
static const double fps      = 1000.0 / timerMs;
static const double totalDist= endX - startX;

SquareWidget::SquareWidget(QWidget *parent)
    : QWidget(parent),
      tcpSocket(new QTcpSocket(this)),
      reconnectTimer(new QTimer(this))
{
    setFocusPolicy(Qt::StrongFocus);

    carPixmapNormal.load("car.png");
    carPixmapSport.load("car2.png");
    carPixmapEmergency.load("car3.png");

    startTimer(int(timerMs));

    // Mantener siempre la impresión de cada línea recibida
    connect(tcpSocket, &QTcpSocket::readyRead, this, &SquareWidget::readServerData);
    connect(tcpSocket,
            QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, &SquareWidget::handleSocketError);
    connect(reconnectTimer, &QTimer::timeout, this, &SquareWidget::tryReconnect);

    tcpSocket->connectToHost("127.0.0.1", 8080);
    reconnectTimer->start(5000);
}

void SquareWidget::readServerData()
{
    while (tcpSocket->canReadLine()) {
        QString message = tcpSocket->readLine().trimmed();
        // **Este qDebug quedó intacto para ver siempre los mensajes**
        qDebug() << "Mensaje recibido:" << message;

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

void SquareWidget::handleSocketError(QAbstractSocket::SocketError)
{
    qWarning() << "Error de conexión:" << tcpSocket->errorString();
    reconnectTimer->start(5000);
}

void SquareWidget::tryReconnect()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
        tcpSocket->abort();
        tcpSocket->connectToHost("127.0.0.1", 8080);
    }
}

void SquareWidget::parseInitialConfig(const QString &msg)
{
    auto ex = [&](const QString &s){
        QString t = s; t.remove('(').remove(')'); return t.trimmed().split(',', Qt::SkipEmptyParts);
    };

    QStringList parts;
    QString token;
    int level = 0;
    for (QChar c : msg) {
        if (c == '(') { if (++level == 1) continue; }
        if (c == ')') { if (--level == 0) break; }
        if (c == ',' && level == 1) {
            parts << token;
            token.clear();
        } else {
            token += c;
        }
    }
    parts << token;

    QStringList tL = ex(parts[2]), tR = ex(parts[3]);
    QStringList iL = ex(parts[4]), iR = ex(parts[5]);
    roundRobinBit  = parts[6].toInt();

    cars.clear();
    leftQueue.clear(); rightQueue.clear();
    leftIds.clear();   rightIds.clear();
    rrProgress.clear();

    for (auto &s : tL) leftQueue.append((CarType)s.toInt());
    for (auto &s : tR) rightQueue.append((CarType)s.toInt());
    for (auto &s : iL) leftIds.append(s.toInt());
    for (auto &s : iR) rightIds.append(s.toInt());

    update();
}

void SquareWidget::parseActionMessage(const QString &msg)
{
    QString clean = msg; clean.remove('(').remove(')');
    QStringList f = clean.split(',', Qt::SkipEmptyParts);
    bool rr4 = (roundRobinBit == 1 && f.size() == 4);
    if (!(f.size() == 3 || rr4)) return;

    int side  = f[0].toInt(), carId = f[1].toInt();
    QVector<int> &ids = (side == 0 ? leftIds : rightIds);
    int idx = ids.indexOf(carId);
    if (idx < 0) return;

    CarType type = (side == 0 ? leftQueue[idx] : rightQueue[idx]);

    if (rr4) {
        // RR parcial
        double quantum = f[2].toDouble();
        double distPx   = f[3].toDouble();
        double prevPx   = rrProgress.value(carId, 0.0);
        double dir      = (side == 0 ? 1.0 : -1.0);
        double frames   = quantum * fps;
        double speed    = (frames > 0 ? distPx / frames : 0.0);

        Car c;
        c.id             = carId;
        c.xPos           = (side == 0 ? startX : endX) + dir * prevPx;
        c.yPos           = fixedY;
        c.type           = type;
        c.movingRight    = (side == 0);
        c.secondsToCross = quantum;
        c.vx             = speed * dir;
        c.rr             = true;
        c.targetX        = c.xPos + dir * distPx;

        cars.append(c);

        // actualiza progreso
        prevPx += distPx;
        rrProgress[carId] = prevPx;

        // si completó, elimina de la cola y progreso
        if (prevPx >= totalDist) {
            if (side == 0) {
                leftQueue.removeAt(idx);
                leftIds.removeAt(idx);
            } else {
                rightQueue.removeAt(idx);
                rightIds.removeAt(idx);
            }
            rrProgress.remove(carId);
        }

    } else {
        // Cruce completo
        double seconds = f[2].toDouble();
        if (side == 0)
            startCarFromLeft(carId, type, seconds);
        else
            startCarFromRight(carId, type, seconds);

        // elimina de la cola
        if (side == 0) {
            leftQueue.removeAt(idx);
            leftIds.removeAt(idx);
        } else {
            rightQueue.removeAt(idx);
            rightIds.removeAt(idx);
        }
    }

    update();
}

void SquareWidget::startCarFromLeft(int carId, CarType type, double seconds)
{
    double frames = seconds * fps;
    double speed  = (frames > 0 ? totalDist / frames : totalDist);

    Car c;
    c.id             = carId;
    c.xPos           = startX;
    c.yPos           = fixedY;
    c.type           = type;
    c.movingRight    = true;
    c.secondsToCross = seconds;
    c.vx             = speed;

    cars.append(c);
    isCrossing = true;
}

void SquareWidget::startCarFromRight(int carId, CarType type, double seconds)
{
    double frames = seconds * fps;
    double speed  = (frames > 0 ? totalDist / frames : totalDist);

    Car c;
    c.id             = carId;
    c.xPos           = endX;       // ← asegurado inicio en endX
    c.yPos           = fixedY;
    c.type           = type;
    c.movingRight    = false;
    c.secondsToCross = seconds;
    c.vx             = -speed;

    cars.append(c);
    isCrossing = true;
}

void SquareWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setPen(QPen(Qt::black,4));
    p.drawLine(startX,200,endX,200);
    p.drawLine(startX,400,endX,400);

    for (auto &c : cars) {
        QPixmap pm = (c.type==Sport ? carPixmapSport
                    : c.type==Emergency ? carPixmapEmergency
                                         : carPixmapNormal);
        if (!c.movingRight) pm = pm.transformed(QTransform().scale(-1,1));
        p.drawPixmap(int(c.xPos), int(c.yPos), 50,50, pm);
    }

    for (int i = 0; i < leftQueue.size(); ++i) {
        QPixmap pm = (leftQueue[i]==Sport ? carPixmapSport
                    : leftQueue[i]==Emergency ? carPixmapEmergency
                                              : carPixmapNormal);
        p.drawPixmap(150, 300 - i*55, 50,50, pm);
    }
    for (int i = 0; i < rightQueue.size(); ++i) {
        QPixmap pm = (rightQueue[i]==Sport ? carPixmapSport
                    : rightQueue[i]==Emergency ? carPixmapEmergency
                                              : carPixmapNormal);
        pm = pm.transformed(QTransform().scale(-1,1));
        p.drawPixmap(1050, 300 - i*55, 50,50, pm);
    }

    p.setFont(QFont("Arial",12));
    p.setPen(Qt::black);
    p.drawText(50,50,"Tipo seleccionado:");
    QPixmap sel = (selectedType==Sport ? carPixmapSport
                : selectedType==Emergency ? carPixmapEmergency
                                          : carPixmapNormal);
    p.drawPixmap(200,25,50,50, sel);
    p.drawText(260,50,
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
        Car &c = cars[i];
        c.xPos += c.vx;

        if (c.rr) {
            if ((c.movingRight && c.xPos >= c.targetX) ||
                (!c.movingRight && c.xPos <= c.targetX)) {
                cars.removeAt(i);
            }
        } else {
            if ((c.movingRight && c.xPos >= endX) ||
                (!c.movingRight && c.xPos <= startX)) {
                cars.removeAt(i);
                isCrossing = false;
            }
        }
    }
    update();
}
