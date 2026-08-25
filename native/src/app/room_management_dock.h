#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

#include "workspace/room_workspace_types.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

class RoomManagementDock final : public QWidget {
    Q_OBJECT

public:
    explicit RoomManagementDock(QWidget *parent = nullptr);

    QLineEdit *roomIdInput() const noexcept;
    QPushButton *addButton() const noexcept;
    QWidget *rowForRoom(const QString &roomId) const noexcept;
    QString feedbackText() const;

public slots:
    void setRooms(const RoomSnapshots &snapshots);
    void setCommandResult(RoomCommandResult result);

signals:
    void addRequested(QString roomId);
    void removeRequested(QString roomId);
    void primaryRequested(QString roomId);
    void requestedQualityChanged(QString roomId, StreamQuality quality);

private:
    void updateAddButtonEnabled();
    void setRowsInteractive(bool interactive);

    QLineEdit *roomIdInput_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QLabel *feedback_ = nullptr;
    QVBoxLayout *rowsLayout_ = nullptr;
    QHash<QString, QWidget *> rows_;
};
