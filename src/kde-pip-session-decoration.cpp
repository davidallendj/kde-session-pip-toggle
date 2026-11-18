#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

class NestedDecoration final : public QWidget {
public:
    NestedDecoration() {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setObjectName(QStringLiteral("kdeToggleNestedDecoration"));
        layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        auto *title = new QPushButton(QStringLiteral("KDE PiP Session"), this);
        title->setFlat(true);
        title->setStyleSheet(QStringLiteral("font-weight: 600;"));
        layout->addWidget(title);
        layout->addStretch();
        auto *settings = new QPushButton(this);
        auto *toggle = new QPushButton(this);
        toggle->setCheckable(true);
        toggle->setChecked(qEnvironmentVariableIsSet("KDE_PIP_SESSION_NESTED_VISIBLE") && qEnvironmentVariable("KDE_PIP_SESSION_NESTED_VISIBLE") == QStringLiteral("1"));
        auto *pin = new QPushButton(this);
        pin->setCheckable(true);
        pin->setChecked(qEnvironmentVariableIsSet("KDE_PIP_SESSION_PINNED") && qEnvironmentVariable("KDE_PIP_SESSION_PINNED") == QStringLiteral("1"));
        auto *hideButton = new QPushButton(this);
        auto setIconOrText = [](QPushButton *button, const QString &iconName, const QString &fallback) {
            const auto icon = QIcon::fromTheme(iconName);
            button->setToolTip(fallback);
            if (icon.isNull()) {
                button->setText(fallback);
            } else {
                button->setIcon(icon);
                button->setAccessibleName(fallback);
            }
        };
        setIconOrText(settings, QStringLiteral("configure"), QStringLiteral("Settings"));
        setIconOrText(toggle, QStringLiteral("view-visible"), QStringLiteral("Toggle"));
        setIconOrText(pin, QStringLiteral("window-pin"), QStringLiteral("Pin"));
        setIconOrText(hideButton, QStringLiteral("window-close"), QStringLiteral("Hide"));
        layout->addWidget(settings);
        layout->addWidget(toggle);
        layout->addWidget(pin);
        layout->addWidget(hideButton);

        resizeHandle = new QWidget(this);
        resizeHandle->setObjectName(QStringLiteral("kdeToggleNestedDecorationResizeHandle"));
        resizeHandle->setToolTip(QStringLiteral("Drag to resize the nested session"));
        resizeHandle->setAccessibleName(QStringLiteral("Resize nested session"));
        resizeHandle->setMouseTracking(true);
        resizeHandle->installEventFilter(this);

        connect(settings, &QPushButton::clicked, this, [] { writeCommand(QStringLiteral("settings")); });
        connect(toggle, &QPushButton::clicked, this, [] { writeCommand(QStringLiteral("toggle")); });
        connect(pin, &QPushButton::toggled, this, [](bool checked) { writeCommand(QStringLiteral("pin ") + (checked ? QStringLiteral("1") : QStringLiteral("0"))); });
        connect(hideButton, &QPushButton::clicked, this, [this] { writeCommand(QStringLiteral("hide-decoration")); hide(); });
        resizeCommandTimer.setSingleShot(true);
        resizeCommandTimer.setInterval(40);
        connect(&resizeCommandTimer, &QTimer::timeout, this, [this] { flushResizeCommand(); });
        setStyleSheet(QStringLiteral("QWidget { background: palette(window); border: 1px solid palette(mid); } QHBoxLayout { margin: 0px; } QPushButton { padding: 3px 8px; border: 1px solid palette(mid); border-radius: 3px; } QPushButton:pressed { background: palette(highlight); color: palette(highlighted-text); } QPushButton:checked { background: palette(highlight); color: palette(highlighted-text); font-weight: 600; } #kdeToggleNestedDecorationResizeHandle { background: palette(mid); }"));
        edgeName = qEnvironmentVariable("KDE_PIP_SESSION_EDGE", "bottom");
        syncState();
        auto *timer = new QTimer(this);
        timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this] { syncState(); });
        timer->start();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != resizeHandle)
            return QWidget::eventFilter(watched, event);

        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() != Qt::LeftButton)
                return true;
            const auto *screen = QGuiApplication::primaryScreen();
            if (!screen)
                return true;
            resizing = true;
            dragStart = mouse->globalPosition().toPoint();
            dragStartDimension = (edgeName == QStringLiteral("left") || edgeName == QStringLiteral("right"))
                                      ? screen->geometry().width()
                                      : screen->geometry().height();
            return true;
        }

        if (event->type() == QEvent::MouseMove && resizing) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const auto delta = mouse->globalPosition().toPoint() - dragStart;
            const bool horizontal = edgeName == QStringLiteral("left") || edgeName == QStringLiteral("right");
            const int signedDelta = edgeName == QStringLiteral("top") || edgeName == QStringLiteral("left")
                                        ? (horizontal ? delta.x() : delta.y())
                                        : -(horizontal ? delta.x() : delta.y());
            const auto *screen = QGuiApplication::primaryScreen();
            const int maximum = screen
                                    ? (horizontal ? screen->geometry().width() : screen->geometry().height())
                                    : dragStartDimension;
            const int minimum = horizontal ? 320 : 200;
            queueResize(horizontal ? QStringLiteral("width") : QStringLiteral("height"),
                        qBound(minimum, dragStartDimension + signedDelta, qMax(minimum, maximum)));
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                resizing = false;
                flushResizeCommand();
            }
            return true;
        }

        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        updateResizeHandleGeometry();
    }

private:
    QBoxLayout *layout = nullptr;
    QWidget *resizeHandle = nullptr;
    QTimer resizeCommandTimer;
    QString edgeName;
    QPoint dragStart;
    int dragStartDimension = 0;
    int pendingResizeDimension = -1;
    QString pendingResizeAxis;
    bool resizing = false;

    static QString commandPath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.command");
    }

    static void writeCommand(const QString &command) {
        QFile file(commandPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((command + QLatin1Char('\n')).toUtf8());
    }

    bool readState(QString *edge, bool *visible) const {
        QFile file(statePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        if (visible)
            *visible = lines.value(0).trimmed() != QStringLiteral("0");
        if (edge)
            *edge = lines.value(1).trimmed();
        return true;
    }

    void syncState() {
        QString nextEdge;
        bool visible = true;
        if (!readState(&nextEdge, &visible)) {
            nextEdge = qEnvironmentVariable("KDE_PIP_SESSION_EDGE", "bottom");
            visible = !qEnvironmentVariableIsSet("KDE_PIP_SESSION_DECORATION_VISIBLE")
                   || qEnvironmentVariable("KDE_PIP_SESSION_DECORATION_VISIBLE") != QStringLiteral("0");
        }
        if (nextEdge.isEmpty())
            nextEdge = QStringLiteral("bottom");
        if (edgeName != nextEdge) {
            edgeName = nextEdge;
            layout->setDirection(edgeName == QStringLiteral("left") || edgeName == QStringLiteral("right")
                                     ? QBoxLayout::TopToBottom
                                     : QBoxLayout::LeftToRight);
        }
        position();
        if (visible) show(); else hide();
    }

    void updateResizeHandleGeometry() {
        if (!resizeHandle)
            return;
        const bool horizontal = edgeName == QStringLiteral("top") || edgeName == QStringLiteral("bottom");
        const int handleThickness = 6;
        if (horizontal) {
            const int y = edgeName == QStringLiteral("top") ? height() - handleThickness : 0;
            resizeHandle->setGeometry(0, qMax(0, y), width(), handleThickness);
            resizeHandle->setCursor(Qt::SizeVerCursor);
        } else {
            const int x = edgeName == QStringLiteral("left") ? width() - handleThickness : 0;
            resizeHandle->setGeometry(qMax(0, x), 0, handleThickness, height());
            resizeHandle->setCursor(Qt::SizeHorCursor);
        }
        resizeHandle->raise();
    }

    void queueResize(const QString &axis, int dimension) {
        pendingResizeAxis = axis;
        pendingResizeDimension = dimension;
        resizeCommandTimer.start();
    }

    void flushResizeCommand() {
        if (pendingResizeDimension < 0 || pendingResizeAxis.isEmpty())
            return;
        writeCommand(QStringLiteral("resize %1 %2").arg(pendingResizeAxis).arg(pendingResizeDimension));
        pendingResizeAxis.clear();
        pendingResizeDimension = -1;
        resizeCommandTimer.stop();
    }

    static QString statePath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.state");
    }

    void position() {
        const auto screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        const QRect screenGeometry = screen->geometry();
        constexpr int thickness = 42;
        QRect barGeometry = screenGeometry;
        if (edgeName == QStringLiteral("bottom")) {
            barGeometry.setTop(screenGeometry.bottom() - thickness + 1);
            barGeometry.setHeight(thickness);
        } else if (edgeName == QStringLiteral("top")) {
            barGeometry.setHeight(thickness);
        } else if (edgeName == QStringLiteral("left")) {
            barGeometry.setWidth(thickness);
        } else {
            barGeometry.setLeft(screenGeometry.right() - thickness + 1);
            barGeometry.setWidth(thickness);
        }
        if (geometry() != barGeometry)
            setGeometry(barGeometry);
        updateResizeHandleGeometry();
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-pip-session-decoration"));
    NestedDecoration decoration;
    return app.exec();
}
