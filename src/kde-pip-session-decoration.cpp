#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QScreen>
#include <QProcess>
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
        edgeName = qEnvironmentVariable("KDE_PIP_SESSION_EDGE", "bottom");
        syncState();
        QTimer::singleShot(0, this, [this] { updateKWinPlacement(); });
        auto *timer = new QTimer(this);
        timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this] { syncState(); });
        timer->start();
    }

    ~NestedDecoration() override {
        unloadKWinPlacement();
        QFile::remove(placementScriptPath());
    }

private:
    QBoxLayout *layout = nullptr;
    QString edgeName;
    QString activePlacementPlugin;
    int nestedWidth = 0;
    int nestedHeight = 0;

    static QString commandPath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.command");
    }

    static void writeCommand(const QString &command) {
        QFile file(commandPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((command + QLatin1Char('\n')).toUtf8());
    }

    bool readState(QString *edge, bool *visible, int *width, int *height, bool *sessionVisible) const {
        QFile file(statePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        if (visible)
            *visible = lines.value(0).trimmed() != QStringLiteral("0");
        if (edge)
            *edge = lines.value(1).trimmed();
        if (width)
            *width = lines.value(2).trimmed().toInt();
        if (height)
            *height = lines.value(3).trimmed().toInt();
        if (sessionVisible)
            *sessionVisible = lines.value(4).trimmed() == QStringLiteral("1");
        return true;
    }

    void syncState() {
        QString nextEdge;
        bool visible = true;
        bool sessionVisible = false;
        int nextWidth = 0;
        int nextHeight = 0;
        if (!readState(&nextEdge, &visible, &nextWidth, &nextHeight, &sessionVisible)) {
            nextEdge = qEnvironmentVariable("KDE_PIP_SESSION_EDGE", "bottom");
            visible = !qEnvironmentVariableIsSet("KDE_PIP_SESSION_DECORATION_VISIBLE")
                   || qEnvironmentVariable("KDE_PIP_SESSION_DECORATION_VISIBLE") != QStringLiteral("0");
            sessionVisible = qEnvironmentVariable("KDE_PIP_SESSION_NESTED_VISIBLE") == QStringLiteral("1");
        }
        if (nextEdge.isEmpty())
            nextEdge = QStringLiteral("bottom");
        const bool placementChanged = edgeName != nextEdge || nestedWidth != nextWidth || nestedHeight != nextHeight;
        if (edgeName != nextEdge) {
            edgeName = nextEdge;
            layout->setDirection(edgeName == QStringLiteral("left") || edgeName == QStringLiteral("right")
                                     ? QBoxLayout::TopToBottom
                                     : QBoxLayout::LeftToRight);
        }
        nestedWidth = nextWidth;
        nestedHeight = nextHeight;
        if (placementChanged)
            updateKWinPlacement();
        position();
        if (visible && sessionVisible) show(); else hide();
    }

    static QString statePath() {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return (runtime.isEmpty() ? QStringLiteral("/tmp") : runtime) + QStringLiteral("/kde-pip-session-decoration.state");
    }

    QString placementScriptPath() const {
        return QDir::temp().filePath(QStringLiteral("kde-pip-session-decoration-%1.js")
                                         .arg(QCoreApplication::applicationPid()));
    }

    void unloadKWinPlacement() {
        if (activePlacementPlugin.isEmpty())
            return;
        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting.unloadScript"), activePlacementPlugin
        });
        activePlacementPlugin.clear();
    }

    void updateKWinPlacement() {
        unloadKWinPlacement();
        const auto plugin = QStringLiteral("kde-pip-session-decoration-%1-%2")
                                .arg(QCoreApplication::applicationPid())
                                .arg(++placementCounter);
        const auto placementScript = QStringLiteral(R"JS(
var targetPid = %1;
var edge = "%2";
var requestedWidth = %3;
var requestedHeight = %4;
var thickness = 42;

function place(window) {
    if (!window || window.pid != targetPid) return;
    window.noBorder = true;
    window.keepAbove = true;
    window.skipTaskbar = true;
    window.skipPager = true;
    window.onAllDesktops = true;
    var area = workspace.clientArea(KWin.FullArea, window);
    var width = (edge === "left" || edge === "right") ? thickness : Math.min(area.width, Math.max(thickness, requestedWidth > 0 ? requestedWidth : area.width));
    var height = (edge === "top" || edge === "bottom") ? thickness : Math.min(area.height, Math.max(thickness, requestedHeight > 0 ? requestedHeight : area.height));
    var x = area.x;
    var y = area.y;
    if (edge === "top" || edge === "bottom") x = area.x + (area.width - width) / 2;
    if (edge === "bottom") y = area.y + area.height - height;
    else if (edge === "left" || edge === "right") y = area.y + (area.height - height) / 2;
    if (edge === "right") x = area.x + area.width - width;
    var geometry = {x: x, y: y, width: width, height: height};
    var current = window.frameGeometry;
    if (current.x !== geometry.x || current.y !== geometry.y ||
        current.width !== geometry.width || current.height !== geometry.height) {
        window.frameGeometry = geometry;
    }
}

workspace.windowList().forEach(place);
workspace.windowAdded.connect(place);
 )JS").arg(QCoreApplication::applicationPid()).arg(edgeName).arg(nestedWidth).arg(nestedHeight);
        QFile file(placementScriptPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return;
        file.write(placementScript.toUtf8());
        file.close();
        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting.loadScript"), placementScriptPath(), plugin
        });
        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting.start")
        });
        activePlacementPlugin = plugin;
    }

    void position() {
        const auto screen = QGuiApplication::primaryScreen();
        if (!screen) return;
        const QRect screenGeometry = screen->geometry();
        constexpr int thickness = 42;
        const bool vertical = edgeName == QStringLiteral("left") || edgeName == QStringLiteral("right");
        const int width = vertical ? thickness : nestedWidth > 0 ? qBound(thickness, nestedWidth, screenGeometry.width()) : screenGeometry.width();
        const int height = vertical ? nestedHeight > 0 ? qBound(thickness, nestedHeight, screenGeometry.height()) : screenGeometry.height() : thickness;
        const int x = edgeName == QStringLiteral("right") ? screenGeometry.right() - width + 1
                        : vertical ? screenGeometry.left()
                                   : screenGeometry.left() + (screenGeometry.width() - width) / 2;
        const int y = edgeName == QStringLiteral("bottom") ? screenGeometry.bottom() - height + 1
                        : vertical ? screenGeometry.top() + (screenGeometry.height() - height) / 2
                                   : screenGeometry.top();
        const QRect barGeometry(x, y, width, height);
        if (geometry() != barGeometry)
            setGeometry(barGeometry);
    }

    int placementCounter = 0;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-pip-session-decoration"));
    NestedDecoration decoration;
    return app.exec();
}
