#ifndef SQUAREWIDGET_H
#define SQUAREWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <vector>

class SquareWidget : public QWidget
{
    Q_OBJECT

public:
    SquareWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    enum CarType { Normal, Sport, Emergency };

    struct Car {
        int xPos;
        int yPos;
        CarType type;
        bool movingRight;
    };

    std::vector<Car> cars;
    std::vector<CarType> leftQueue;
    std::vector<CarType> rightQueue;

    QPixmap carPixmapNormal;
    QPixmap carPixmapSport;
    QPixmap carPixmapEmergency;

    CarType selectedType = Normal;

    bool isCrossing = false;

    void initializeQueues();
    void startCarFromLeft();
    void startCarFromRight();
};

#endif // SQUAREWIDGET_H
