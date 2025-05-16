// SquareWidget.h
#ifndef SQUAREWIDGET_H
#define SQUAREWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QVector>
#include <QMap>
#include <QTcpSocket>
#include <QTimer>
#include <QAbstractSocket>

class SquareWidget : public QWidget {
    Q_OBJECT

public:
    explicit SquareWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private slots:
    void readServerData();
    void handleSocketError(QAbstractSocket::SocketError error);
    void tryReconnect();

private:
    enum CarType { Normal=0, Sport=1, Emergency=2 };

    struct Car {
        int    id;
        double xPos, yPos;
        CarType type;
        bool movingRight;
        double secondsToCross;
        double vx;
        bool   rr       = false;
        double targetX  = 0.0;
    };

    // Networking
    QTcpSocket* tcpSocket;
    QTimer*     reconnectTimer;

    // State
    QVector<Car>    cars;
    bool            isCrossing = false;

    // Queues & IDs
    QVector<CarType> leftQueue, rightQueue;
    QVector<int>     leftIds, rightIds;
    int              roundRobinBit = 0;

    // RR progress
    QMap<int,double> rrProgress;

    // Pixmaps
    QPixmap carPixmapNormal, carPixmapSport, carPixmapEmergency;
    CarType selectedType = Normal;

    // Helpers
    void parseInitialConfig(const QString &msg);
    void parseActionMessage(const QString &msg);
    void startCarFromLeft(int carId, CarType type, double seconds);
    void startCarFromRight(int carId, CarType type, double seconds);
};

#endif // SQUAREWIDGET_H
