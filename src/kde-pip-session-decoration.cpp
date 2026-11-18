#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
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
        connect(settings, &QPushButton::clicked, this, [] { writeCommand(QStringLiteral("settings")); });
        connect(toggle, &QPushButton::clicked, this, [] { writeCommand(QStringLiteral("toggle")); });
        connect(pin, &QPushButton::toggled, this, [](bool checked) { writeCommand(QStringLiteral("pin ") + (checked ? QStringLiteral("1") : QStringLiteral("0"))); });
        connect(hideButton, &QPushButton::clicked, this, [this] { writeCommand(QStringLiteral("hide-decoration")); hide(); });
        setStyleSheet(QStringLiteral("QWidget { background: palette(window); border: 1px solid palette(mid); } QHBoxLayout { margin: 0px; } QPushButton { padding: 3px 8px; border: 1px solid palette(mid); border-radius: 3px; } QPushButton:pressed { background: palette(highlight); color: palette(highlighted-text); } QPushButton:checked { background: palette(highlight); color: palette(highlighted-text); font-weight: 600; }"));
        position();
        const bool visible = !qEnvironmentVariableIsSet("KDE_PIP_SESSION_DECORATION_VISIBLE") || qEnvironmentVariable("KDE_PIP_SESSION_DECORATION_VISIBLE") != QStringLiteral("0");
        if (visible) show();
        auto *timer = new QTimer(this);
        timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this] { position(); syncVisibility(); });
        timer->start();
    }

private:
    QBoxLayout *layout = nullptr;

    static QString commandPath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.command");
    }

    static void writeCommand(const QString &command) {
        QFile file(commandPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((command + QLatin1Char('\n')).toUtf8());
    }

    void syncVisibility() {
        QFile file(statePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        const bool visible = lines.value(0).trimmed() != QStringLiteral("0");
        const auto edge = lines.value(1).trimmed();
        layout->setDirection(edge == QStringLiteral("left") || edge == QStringLiteral("right")
                                 ? QBoxLayout::TopToBottom
                                 : QBoxLayout::LeftToRight);
        if (visible) show(); else hide();
    }

    static QString statePath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.state");
    }

    void position() {
        const auto screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        QFile file(statePath());
        QString edge = qEnvironmentVariable("KDE_PIP_SESSION_EDGE", "bottom");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
            if (!lines.value(1).trimmed().isEmpty())
                edge = lines.value(1).trimmed();
        }
        const auto screenGeometry = screen->geometry();
        if (edge == QStringLiteral("bottom"))
            setGeometry(screenGeometry.x(), screenGeometry.y(), screenGeometry.width(), 42);
        else if (edge == QStringLiteral("top"))
            setGeometry(screenGeometry.x(), screenGeometry.bottom() - 41, screenGeometry.width(), 42);
        else if (edge == QStringLiteral("left"))
            setGeometry(screenGeometry.right() - 41, screenGeometry.y(), 42, screenGeometry.height());
        else
            setGeometry(screenGeometry.x(), screenGeometry.y(), 42, screenGeometry.height());
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-pip-session-decoration"));
    NestedDecoration decoration;
    return app.exec();
}
