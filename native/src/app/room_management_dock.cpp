#include "app/room_management_dock.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "workspace/multi_room_coordinator.h"

namespace {

QString qualityText(StreamQuality quality)
{
    switch (quality) {
    case StreamQuality::Auto:
        return QStringLiteral("Auto");
    case StreamQuality::Original:
        return QStringLiteral("Original");
    case StreamQuality::Super:
        return QStringLiteral("Super");
    case StreamQuality::High:
        return QStringLiteral("High");
    case StreamQuality::Standard:
        return QStringLiteral("Standard");
    }
    return QStringLiteral("Auto");
}

QString stateText(RoomSession::State state)
{
    switch (state) {
    case RoomSession::State::Idle:
        return QStringLiteral("Idle");
    case RoomSession::State::Resolving:
        return QStringLiteral("Resolving");
    case RoomSession::State::Ready:
        return QStringLiteral("Ready");
    case RoomSession::State::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Idle");
}

QString commandFeedbackText(RoomCommandResult result)
{
    switch (result) {
    case RoomCommandResult::InvalidRoomId:
        return QStringLiteral("Invalid room ID");
    case RoomCommandResult::DuplicateRoomId:
        return QStringLiteral("Room already exists");
    case RoomCommandResult::RoomLimitReached:
        return QStringLiteral("Maximum of 9 rooms reached");
    case RoomCommandResult::RoomNotFound:
        return QStringLiteral("Room is no longer managed");
    case RoomCommandResult::AlreadyPrimary:
        return QStringLiteral("Room is already primary");
    case RoomCommandResult::Unavailable:
        return QStringLiteral("Room management is unavailable");
    case RoomCommandResult::Accepted:
    case RoomCommandResult::Unchanged:
        return QString();
    }
    return QString();
}

class RoomManagementRow final : public QWidget {
public:
    explicit RoomManagementRow(const QString &roomId, QWidget *parent = nullptr)
        : QWidget(parent)
        , roomId_(roomId)
    {
        setObjectName(QStringLiteral("roomRow-") + roomId_);
        setFixedHeight(44);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(6);

        auto *roomIdLabel = new QLabel(roomId_, this);
        roomIdLabel->setMinimumWidth(72);
        layout->addWidget(roomIdLabel);

        stateLabel_ = new QLabel(this);
        stateLabel_->setMinimumWidth(68);
        layout->addWidget(stateLabel_);

        primaryButton_ = new QToolButton(this);
        primaryButton_->setObjectName(QStringLiteral("primaryButton"));
        primaryButton_->setCheckable(true);
        primaryButton_->setAutoRaise(true);
        primaryButton_->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
        primaryButton_->setToolTip(QStringLiteral("Set primary room"));
        primaryButton_->setAccessibleName(QStringLiteral("Set primary room"));
        layout->addWidget(primaryButton_);

        qualityCombo_ = new QComboBox(this);
        qualityCombo_->setObjectName(QStringLiteral("qualityCombo"));
        qualityCombo_->setAccessibleName(QStringLiteral("Requested quality"));
        addQuality(StreamQuality::Auto);
        addQuality(StreamQuality::Original);
        addQuality(StreamQuality::Super);
        addQuality(StreamQuality::High);
        addQuality(StreamQuality::Standard);
        layout->addWidget(qualityCombo_);

        effectiveQualityLabel_ = new QLabel(this);
        effectiveQualityLabel_->setObjectName(QStringLiteral("effectiveQualityLabel"));
        effectiveQualityLabel_->setMinimumWidth(118);
        layout->addWidget(effectiveQualityLabel_);

        policyLabel_ = new QLabel(this);
        policyLabel_->setObjectName(QStringLiteral("policyLabel"));
        policyLabel_->setMinimumWidth(82);
        layout->addWidget(policyLabel_);

        removeButton_ = new QToolButton(this);
        removeButton_->setObjectName(QStringLiteral("removeButton"));
        removeButton_->setAutoRaise(true);
        removeButton_->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
        removeButton_->setToolTip(QStringLiteral("Remove room"));
        removeButton_->setAccessibleName(QStringLiteral("Remove room"));
        layout->addWidget(removeButton_);
    }

    QString roomId() const
    {
        return roomId_;
    }

    QToolButton *primaryButton() const noexcept
    {
        return primaryButton_;
    }

    QComboBox *qualityCombo() const noexcept
    {
        return qualityCombo_;
    }

    QToolButton *removeButton() const noexcept
    {
        return removeButton_;
    }

    void apply(const RoomSnapshot &snapshot)
    {
        const QSignalBlocker primaryBlocker(primaryButton_);
        const QSignalBlocker qualityBlocker(qualityCombo_);

        stateLabel_->setText(stateText(snapshot.state));
        primaryButton_->setChecked(snapshot.isPrimary);
        qualityCombo_->setCurrentIndex(
            qualityCombo_->findData(static_cast<int>(snapshot.requestedQuality)));
        effectiveQualityLabel_->setText(
            QStringLiteral("Effective: ") + qualityText(snapshot.effectiveQuality));
        policyLabel_->setText(snapshot.requestedQuality == snapshot.effectiveQuality
                                  ? QString()
                                  : QStringLiteral("Policy active"));
    }

    void setInteractive(bool interactive)
    {
        primaryButton_->setEnabled(interactive);
        qualityCombo_->setEnabled(interactive);
        removeButton_->setEnabled(interactive);
    }

private:
    void addQuality(StreamQuality quality)
    {
        qualityCombo_->addItem(qualityText(quality), static_cast<int>(quality));
    }

    QString roomId_;
    QLabel *stateLabel_ = nullptr;
    QToolButton *primaryButton_ = nullptr;
    QComboBox *qualityCombo_ = nullptr;
    QLabel *effectiveQualityLabel_ = nullptr;
    QLabel *policyLabel_ = nullptr;
    QToolButton *removeButton_ = nullptr;
};

RoomManagementRow *asRoomRow(QWidget *widget)
{
    return static_cast<RoomManagementRow *>(widget);
}

} // namespace

RoomManagementDock::RoomManagementDock(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto *entryLayout = new QHBoxLayout;
    entryLayout->setContentsMargins(0, 0, 0, 0);
    roomIdInput_ = new QLineEdit(this);
    roomIdInput_->setObjectName(QStringLiteral("roomIdInput"));
    roomIdInput_->setAccessibleName(QStringLiteral("Room ID"));
    roomIdInput_->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[0-9]{1,20}$")),
                                        roomIdInput_));
    entryLayout->addWidget(roomIdInput_);

    addButton_ = new QPushButton(QStringLiteral("Add"), this);
    addButton_->setObjectName(QStringLiteral("addRoomButton"));
    addButton_->setAccessibleName(QStringLiteral("Add room"));
    entryLayout->addWidget(addButton_);
    layout->addLayout(entryLayout);

    feedback_ = new QLabel(this);
    feedback_->setObjectName(QStringLiteral("roomFeedback"));
    feedback_->setWordWrap(true);
    layout->addWidget(feedback_);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto *rowsHost = new QWidget(scrollArea);
    rowsLayout_ = new QVBoxLayout(rowsHost);
    rowsLayout_->setContentsMargins(0, 0, 0, 0);
    rowsLayout_->setSpacing(2);
    rowsLayout_->addStretch();
    scrollArea->setWidget(rowsHost);
    layout->addWidget(scrollArea, 1);

    connect(roomIdInput_, &QLineEdit::textChanged,
            this, &RoomManagementDock::updateAddButtonEnabled);
    connect(addButton_, &QPushButton::clicked, this, [this] {
        if (!addButton_->isEnabled()) return;
        addButton_->setEnabled(false);
        emit addRequested(roomIdInput_->text());
    });

    updateAddButtonEnabled();
}

QLineEdit *RoomManagementDock::roomIdInput() const noexcept
{
    return roomIdInput_;
}

QPushButton *RoomManagementDock::addButton() const noexcept
{
    return addButton_;
}

QWidget *RoomManagementDock::rowForRoom(const QString &roomId) const noexcept
{
    return rows_.value(roomId, nullptr);
}

QString RoomManagementDock::feedbackText() const
{
    return feedback_->text();
}

void RoomManagementDock::setRooms(const RoomSnapshots &snapshots)
{
    QSet<QString> retainedRoomIds;
    retainedRoomIds.reserve(snapshots.size());
    for (const RoomSnapshot &snapshot : snapshots) {
        retainedRoomIds.insert(snapshot.roomId);
    }

    for (auto it = rows_.begin(); it != rows_.end();) {
        if (retainedRoomIds.contains(it.key())) {
            ++it;
            continue;
        }
        QWidget *row = it.value();
        rowsLayout_->removeWidget(row);
        delete row;
        it = rows_.erase(it);
    }

    for (qsizetype index = 0; index < snapshots.size(); ++index) {
        const RoomSnapshot &snapshot = snapshots.at(index);
        auto *row = asRoomRow(rows_.value(snapshot.roomId, nullptr));
        if (row == nullptr) {
            row = new RoomManagementRow(snapshot.roomId, this);
            rows_.insert(snapshot.roomId, row);

            connect(row->primaryButton(), &QToolButton::clicked, this, [this, row] {
                row->setInteractive(false);
                emit primaryRequested(row->roomId());
            });
            connect(row->qualityCombo(), qOverload<int>(&QComboBox::currentIndexChanged),
                    this, [this, row](int comboIndex) {
                        if (comboIndex < 0) return;
                        row->setInteractive(false);
                        const auto quality = static_cast<StreamQuality>(
                            row->qualityCombo()->itemData(comboIndex).toInt());
                        emit requestedQualityChanged(row->roomId(), quality);
                    });
            connect(row->removeButton(), &QToolButton::clicked, this, [this, row] {
                row->setInteractive(false);
                emit removeRequested(row->roomId());
            });
        }

        row->apply(snapshot);
        rowsLayout_->removeWidget(row);
        rowsLayout_->insertWidget(index, row);
    }

    updateAddButtonEnabled();
}

void RoomManagementDock::setCommandResult(RoomCommandResult result)
{
    feedback_->setText(commandFeedbackText(result));
    setRowsInteractive(true);
    if (result == RoomCommandResult::Accepted) {
        roomIdInput_->clear();
        roomIdInput_->setFocus();
    }
    updateAddButtonEnabled();
}

void RoomManagementDock::updateAddButtonEnabled()
{
    const bool canAdd = roomIdInput_->hasAcceptableInput()
        && rows_.size() < MultiRoomCoordinator::kMaxRooms;
    addButton_->setEnabled(canAdd);
}

void RoomManagementDock::setRowsInteractive(bool interactive)
{
    for (QWidget *widget : rows_) {
        asRoomRow(widget)->setInteractive(interactive);
    }
}
