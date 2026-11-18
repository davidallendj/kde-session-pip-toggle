#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
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
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        auto *title = new QPushButton(QStringLiteral("KDE PiP Session"), this);
        title->setFlat(true);
        title->setStyleSheet(QStringLiteral("font-weight: 600;"));
        layout->addWidget(title);
        layout->addStretch();
        auto *settings = new QPushButton(QStringLiteral("Settings"), this);
        auto *toggle = new QPushButton(QStringLiteral("Toggle"), this);
        toggle->setCheckable(true);
        toggle->setChecked(qEnvironmentVariableIsSet("KDE_PIP_SESSION_NESTED_VISIBLE") && qEnvironmentVariable("KDE_PIP_SESSION_NESTED_VISIBLE") == QStringLiteral("1"));
        auto *pin = new QPushButton(QStringLiteral("Pin"), this);
        pin->setCheckable(true);
        pin->setChecked(qEnvironmentVariableIsSet("KDE_PIP_SESSION_PINNED") && qEnvironmentVariable("KDE_PIP_SESSION_PINNED") == QStringLiteral("1"));
        auto *hideButton = new QPushButton(QStringLiteral("Hide"), this);
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
        timer->setInterval(500);
        connect(timer, &QTimer::timeout, this, [this] { position(); syncVisibility(); });
        timer->start();
    }

private:
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
        const bool visible = QString::fromUtf8(file.readAll()).trimmed() != QStringLiteral("0");
        if (visible) show(); else hide();
    }

    static QString statePath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.state");
    }

    void position() {
        const auto screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        const int width = screen->geometry().width();
        setGeometry(screen->geometry().x(), screen->geometry().y(), width, 42);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-pip-session-decoration"));
    NestedDecoration decoration;
    return app.exec();
}
