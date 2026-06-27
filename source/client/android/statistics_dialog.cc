//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/android/statistics_dialog.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QTime>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/net/udp_channel.h"
#include "common/android/button.h"
#include "common/android/controls.h"
#include "common/android/tree_widget.h"
#include "common/desktop/formatter.h"
#include "proto/desktop_video.h"

namespace {

// Row order in the tree. Unlike the desktop dialog the Android session tracks no mouse/key/text
// counters, so those rows are omitted.
enum Row
{
    ROW_DURATION,
    ROW_TOTAL_TCP,
    ROW_SPEED_TCP,
    ROW_TOTAL_UDP,
    ROW_SPEED_UDP,
    ROW_UDP,
    ROW_VIDEO_PACKET_COUNT,
    ROW_VIDEO_PACKET_STATS,
    ROW_AUDIO_PACKET_COUNT,
    ROW_AUDIO_PACKET_STATS,
    ROW_VIDEO_CODEC,
    ROW_FPS,
    ROW_CLIPBOARD,
    ROW_CURSOR_SHAPE,
    ROW_CURSOR_CACHE_SIZE,
    ROW_CURSOR_POS
};

// The value string is stored next to the parameter name under this role and painted by the delegate.
constexpr int kValueRole = Qt::UserRole;

// Fraction of the screen height the tree may take before it starts scrolling.
constexpr double kTreeHeightFactor = 0.6;

constexpr double kNameFontScale = 0.78;
constexpr int kRowHPadding = 8;
constexpr int kRowVPadding = 5;
constexpr int kLineSpacing = 1;

// Stacks the parameter name (small, muted) over its value on the full card width, so neither has to be
// squeezed into a narrow column. Compact enough that the whole list fits without tall touch rows.
class StatisticsDelegate final : public QStyledItemDelegate
{
public:
    explicit StatisticsDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {
        // Nothing.
    }

    // QStyledItemDelegate implementation.
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /* index */) const final
    {
        const QFont name_font = Controls::scaledFont(option.font, kNameFontScale);
        const int height = QFontMetrics(name_font).height() + kLineSpacing +
            QFontMetrics(option.font).height() + 2 * kRowVPadding;
        return QSize(0, height);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const final
    {
        painter->save();

        const QRect content = option.rect.adjusted(kRowHPadding, kRowVPadding, -kRowHPadding,
                                                   -kRowVPadding);

        const QFont name_font = Controls::scaledFont(option.font, kNameFontScale);
        const QFontMetrics name_metrics(name_font);
        QColor name_color = option.palette.color(QPalette::WindowText);
        name_color.setAlphaF(0.6);

        const QRect name_rect(content.left(), content.top(), content.width(), name_metrics.height());
        painter->setFont(name_font);
        painter->setPen(name_color);
        painter->drawText(name_rect, Qt::AlignLeft | Qt::AlignVCenter, name_metrics.elidedText(
            index.data(Qt::DisplayRole).toString(), Qt::ElideRight, content.width()));

        const QFontMetrics value_metrics(option.font);
        const QRect value_rect(content.left(), name_rect.bottom() + kLineSpacing, content.width(),
                               value_metrics.height());
        painter->setFont(option.font);
        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(value_rect, Qt::AlignLeft | Qt::AlignVCenter, value_metrics.elidedText(
            index.data(kValueRole).toString(), Qt::ElideRight, content.width()));

        painter->restore();
    }
};

//--------------------------------------------------------------------------------------------------
QString capturerToString(quint32 type)
{
    switch (static_cast<proto::video::ScreenCapturerType>(type))
    {
        case proto::video::SCREEN_CAPTURER_TYPE_DEFAULT: return "DEFAULT";
        case proto::video::SCREEN_CAPTURER_TYPE_FAKE: return "FAKE";
        case proto::video::SCREEN_CAPTURER_TYPE_WIN_GDI: return "WIN_GDI";
        case proto::video::SCREEN_CAPTURER_TYPE_WIN_DXGI: return "WIN_DXGI";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_X11: return "LINUX_X11";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_WAYLAND: return "LINUX_WAYLAND";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_KMS: return "LINUX_KMS";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_KWIN: return "LINUX_KWIN";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_WLR: return "LINUX_WLR";
        case proto::video::SCREEN_CAPTURER_TYPE_LINUX_VT: return "LINUX_VT";
        case proto::video::SCREEN_CAPTURER_TYPE_MACOSX: return "MACOSX";
        default: return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString encoderToString(quint32 type, bool hardware_decoder)
{
    switch (static_cast<proto::video::Encoding>(type))
    {
        case proto::video::ENCODING_VP8: return "VP8";
        case proto::video::ENCODING_VP9: return "VP9";
        case proto::video::ENCODING_H264: return hardware_decoder ? "H264HW" : "H264SW";
        default: return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
QString udpMethodToString(UdpMethod method)
{
    switch (method)
    {
        case UdpMethod::DIRECT: return "Direct";
        case UdpMethod::GATEWAY_HOST: return "Gateway (host)";
        case UdpMethod::GATEWAY_CLIENT: return "Gateway (client)";
        case UdpMethod::HOLE_PUNCHING: return "Hole punching";
        default: return "Disabled";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
StatisticsDialog::StatisticsDialog(QWidget* parent)
    : Dialog(parent),
      tree_(new TreeWidget())
{
    setTitle(tr("Statistics"));

    tree_->setColumnCount(1);
    tree_->setRootIsDecorated(false);
    tree_->setSelectionMode(QAbstractItemView::NoSelection);
    tree_->setItemDelegate(new StatisticsDelegate(tree_));

    addRow("Duration");
    addRow("Total TCP RX/TX");
    addRow("Speed TCP RX/TX");
    addRow("Total UDP RX/TX");
    addRow("Speed UDP RX/TX");
    addRow("UDP");
    addRow("Video Packet Count");
    addRow("Video Packet MIN/MAX/AVG");
    addRow("Audio Packet Count");
    addRow("Audio Packet MIN/MAX/AVG");
    addRow("Video Capturer/Codec");
    addRow("FPS");
    addRow("Clipboard Event Read/Send");
    addRow("Cursor Shape Count/Cache");
    addRow("Cursor Shape Cache Size");
    addRow("Cursor Pos Count");

    // Size the tree to its content, but never past a fraction of the screen so it stays on-screen and
    // scrolls for the overflow.
    const int row_height = tree_->sizeHintForRow(0);
    const int content_height = row_height * rows_.size() + 2;
    const int screen_height = QGuiApplication::primaryScreen()->availableGeometry().height();
    tree_->setFixedHeight(qMin(content_height, static_cast<int>(screen_height * kTreeHeightFactor)));

    contentLayout()->addWidget(tree_);

    Button* close = addButton(tr("Close"), Button::Role::FILLED);
    connect(close, &Button::clicked, this, &QDialog::accept);

    QTimer* update_timer = new QTimer(this);
    connect(update_timer, &QTimer::timeout, this, &StatisticsDialog::sig_metricsRequired);
    update_timer->start(std::chrono::seconds(1));
}

//--------------------------------------------------------------------------------------------------
StatisticsDialog::~StatisticsDialog() = default;

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* StatisticsDialog::addRow(const QString& name)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(tree_);
    item->setText(0, name);
    rows_.append(item);
    return item;
}

//--------------------------------------------------------------------------------------------------
void StatisticsDialog::setMetrics(const ClientDesktop::Metrics& metrics)
{
    auto set = [this](int row, const QString& value) { rows_[row]->setData(0, kValueRole, value); };

    set(ROW_DURATION, QTime(0, 0, 0).addSecs(static_cast<int>(metrics.duration.count())).toString());
    set(ROW_TOTAL_TCP, Formatter::sizeToString(metrics.total_tcp_rx) + " / " +
        Formatter::sizeToString(metrics.total_tcp_tx));
    set(ROW_SPEED_TCP, Formatter::transferSpeedToString(metrics.speed_tcp_rx) + " / " +
        Formatter::transferSpeedToString(metrics.speed_tcp_tx));
    set(ROW_TOTAL_UDP, Formatter::sizeToString(metrics.total_udp_rx) + " / " +
        Formatter::sizeToString(metrics.total_udp_tx));
    set(ROW_SPEED_UDP, Formatter::transferSpeedToString(metrics.speed_udp_rx) + " / " +
        Formatter::transferSpeedToString(metrics.speed_udp_tx));
    set(ROW_UDP, udpMethodToString(metrics.udp_method));
    set(ROW_VIDEO_PACKET_COUNT, QString::number(metrics.video_packet_count));
    set(ROW_VIDEO_PACKET_STATS, Formatter::sizeToString(metrics.min_video_packet) + " / " +
        Formatter::sizeToString(metrics.max_video_packet) + " / " +
        Formatter::sizeToString(metrics.avg_video_packet));
    set(ROW_AUDIO_PACKET_COUNT, QString::number(metrics.audio_packet_count));
    set(ROW_AUDIO_PACKET_STATS, Formatter::sizeToString(metrics.min_audio_packet) + " / " +
        Formatter::sizeToString(metrics.max_audio_packet) + " / " +
        Formatter::sizeToString(metrics.avg_audio_packet));
    set(ROW_VIDEO_CODEC, capturerToString(metrics.video_capturer_type) + " / " +
        encoderToString(metrics.video_encoder_type, metrics.video_decoder_hardware));
    set(ROW_FPS, QString::number(metrics.fps));
    set(ROW_CLIPBOARD, QString::number(metrics.read_clipboard) + " / " +
        QString::number(metrics.send_clipboard));
    set(ROW_CURSOR_SHAPE, QString::number(metrics.cursor_shape_count) + " / " +
        QString::number(metrics.cursor_taken_from_cache));
    set(ROW_CURSOR_CACHE_SIZE, QString::number(metrics.cursor_cached));
    set(ROW_CURSOR_POS, QString::number(metrics.cursor_pos_count));
}
