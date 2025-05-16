// SquareWidget.h
#ifndef SQUAREWIDGET_H
#define SQUAREWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QVector>
#include <QTcpSocket>
#include <QTimer>
#include <QAbstractSocket>

class SquareWidget : public QWidget
{
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
    enum CarType { Normal = 0, Sport = 1, Emergency = 2 };

    struct Car {
        int xPos;
        int yPos;
        CarType type;
        bool movingRight;
    };

    // Networking
    QTcpSocket *tcpSocket;
    QTimer     *reconnectTimer;

    // Cars & state
    QVector<Car> cars;
    bool          isCrossing = false;

    // Queues & IDs
    QVector<CarType> leftQueue;
    QVector<CarType> rightQueue;
    QVector<int>     leftIds;
    QVector<int>     rightIds;
    int              roundRobinBit = 0;

    // Pixmaps
    QPixmap carPixmapNormal;
    QPixmap carPixmapSport;
    QPixmap carPixmapEmergency;
    CarType selectedType = Normal;

    // Parsing
    void parseInitialConfig(const QString &msg);
    void parseActionMessage(const QString &msg);

    // Start helpers
    void startCarFromLeft(CarType type);
    void startCarFromRight(CarType type);
};

#endif // SQUAREWIDGET_H