#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <KGlobalAccel>
#include <csignal>
#include <unistd.h>

class ToggleWindow final : public QMainWindow {
public:
    ToggleWindow() {
        setWindowTitle(QStringLiteral("KDE Toggle"));
        setMinimumWidth(480);
        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);
        auto *intro = new QLabel(QStringLiteral("A Yakuake-like global toggle for a virtual desktop, application, or nested Plasma desktop."), central);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto *settingsBox = new QGroupBox(QStringLiteral("Target"), central);
        auto *form = new QFormLayout(settingsBox);
        mode = new QComboBox(settingsBox);
        mode->addItem(QStringLiteral("Virtual desktop"), QStringLiteral("desktop"));
        mode->addItem(QStringLiteral("Application"), QStringLiteral("application"));
        mode->addItem(QStringLiteral("Nested Plasma desktop"), QStringLiteral("nested"));
        form->addRow(QStringLiteral("Mode:"), mode);

        desktop = new QSpinBox(settingsBox);
        desktop->setRange(1, 99);
        desktop->setValue(2);
        form->addRow(QStringLiteral("Desktop number:"), desktop);

        command = new QLineEdit(settingsBox);
        command->setPlaceholderText(QStringLiteral("e.g. konsole --new-window"));
        form->addRow(QStringLiteral("Application command:"), command);

        const QRect screenArea = QGuiApplication::primaryScreen()
                                     ? QGuiApplication::primaryScreen()->availableGeometry()
                                     : QRect(0, 0, 3840, 2160);
        const int minNestedWidth = 320;
        const int minNestedHeight = 200;
        screenWidthMax = qMax(minNestedWidth, screenArea.width());
        screenHeightMax = qMax(minNestedHeight, screenArea.height());
        const int maxNestedWidth = screenWidthMax;
        const int maxNestedHeight = screenHeightMax;

        nestedWidth = new QSpinBox(settingsBox);
        nestedWidth->setRange(minNestedWidth, maxNestedWidth);
        nestedWidth->setSingleStep(80);
        nestedWidth->setValue(1280);
        nestedWidthSlider = new QSlider(Qt::Horizontal, settingsBox);
        nestedWidthSlider->setRange(minNestedWidth, maxNestedWidth);
        nestedWidthSlider->setSingleStep(80);
        nestedWidthSlider->setPageStep(160);
        nestedWidthSlider->setValue(nestedWidth->value());
        auto *widthControls = new QWidget(settingsBox);
        auto *widthLayout = new QHBoxLayout(widthControls);
        widthLayout->setContentsMargins(0, 0, 0, 0);
        widthLayout->addWidget(nestedWidth);
        widthLayout->addWidget(nestedWidthSlider, 1);
        form->addRow(QStringLiteral("Nested width:"), widthControls);

        nestedHeight = new QSpinBox(settingsBox);
        nestedHeight->setRange(minNestedHeight, maxNestedHeight);
        nestedHeight->setSingleStep(60);
        nestedHeight->setValue(800);
        nestedHeightSlider = new QSlider(Qt::Horizontal, settingsBox);
        nestedHeightSlider->setRange(minNestedHeight, maxNestedHeight);
        nestedHeightSlider->setSingleStep(60);
        nestedHeightSlider->setPageStep(120);
        nestedHeightSlider->setValue(nestedHeight->value());
        auto *heightControls = new QWidget(settingsBox);
        auto *heightLayout = new QHBoxLayout(heightControls);
        heightLayout->setContentsMargins(0, 0, 0, 0);
        heightLayout->addWidget(nestedHeight);
        heightLayout->addWidget(nestedHeightSlider, 1);
        form->addRow(QStringLiteral("Nested height:"), heightControls);

        sizePercent = new QCheckBox(QStringLiteral("Use percentage of screen"), settingsBox);
        form->addRow(QString(), sizePercent);

        edge = new QComboBox(settingsBox);
        edge->addItem(QStringLiteral("Bottom"), QStringLiteral("bottom"));
        edge->addItem(QStringLiteral("Top"), QStringLiteral("top"));
        edge->addItem(QStringLiteral("Left"), QStringLiteral("left"));
        edge->addItem(QStringLiteral("Right"), QStringLiteral("right"));
        form->addRow(QStringLiteral("Slide from:"), edge);

        animateSlide = new QCheckBox(QStringLiteral("Animate slide"), settingsBox);
        animateSlide->setChecked(true);
        form->addRow(QString(), animateSlide);

        slideDuration = new QSpinBox(settingsBox);
        slideDuration->setRange(50, 2000);
        slideDuration->setSingleStep(50);
        slideDuration->setSuffix(QStringLiteral(" ms"));
        form->addRow(QStringLiteral("Animation duration:"), slideDuration);

        shortcutEdit = new QKeySequenceEdit(settingsBox);
        shortcutEdit->setToolTip(QStringLiteral("Press the desired global shortcut here."));
        form->addRow(QStringLiteral("Global shortcut:"), shortcutEdit);
        layout->addWidget(settingsBox);

        auto *actions = new QHBoxLayout();
        auto *toggleButton = new QPushButton(QStringLiteral("Toggle target"), central);
        auto *hideButton = new QPushButton(QStringLiteral("Hide settings"), central);
        actions->addWidget(toggleButton);
        actions->addWidget(hideButton);
        layout->addLayout(actions);
        status = new QLabel(central);
        status->setWordWrap(true);
        layout->addWidget(status);
        setCentralWidget(central);

        tray = new QSystemTrayIcon(this);
        tray->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop")));
        auto *trayMenu = new QMenu(this);
        trayMenu->addAction(QStringLiteral("Toggle target"), this, &ToggleWindow::toggleTarget);
        trayMenu->addAction(QStringLiteral("Show settings"), this, [this] { show(); raise(); activateWindow(); });
        trayMenu->addSeparator();
        trayMenu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);
        tray->setContextMenu(trayMenu);
        tray->setToolTip(QStringLiteral("KDE Toggle"));
        tray->show();

        loadSettings();
        connect(mode, &QComboBox::currentIndexChanged, this, [this] { saveSettings(); updateUi(); });
        connect(desktop, &QSpinBox::valueChanged, this, [this] { saveSettings(); });
        connect(command, &QLineEdit::editingFinished, this, [this] { saveSettings(); });
        connect(nestedWidth, &QSpinBox::valueChanged, this, [this](int value) {
            nestedWidthSlider->setValue(value);
            saveSettings();
            resizeNestedWindow();
        });
        connect(nestedWidthSlider, &QSlider::valueChanged, this, [this](int value) {
            const QSignalBlocker blocker(nestedWidth);
            nestedWidth->setValue(value);
        });
        connect(nestedWidthSlider, &QSlider::sliderReleased, this, [this] {
            saveSettings();
            resizeNestedWindow();
        });
        connect(nestedHeight, &QSpinBox::valueChanged, this, [this](int value) {
            nestedHeightSlider->setValue(value);
            saveSettings();
            resizeNestedWindow();
        });
        connect(nestedHeightSlider, &QSlider::valueChanged, this, [this](int value) {
            const QSignalBlocker blocker(nestedHeight);
            nestedHeight->setValue(value);
        });
        connect(nestedHeightSlider, &QSlider::sliderReleased, this, [this] {
            saveSettings();
            resizeNestedWindow();
        });
        connect(sizePercent, &QCheckBox::toggled, this, [this](bool checked) {
            setSizePercentMode(checked);
            saveSettings();
            resizeNestedWindow();
        });
        connect(edge, &QComboBox::currentIndexChanged, this, [this] {
            saveSettings();
            resizeNestedWindow();
        });
        connect(animateSlide, &QCheckBox::toggled, this, [this] {
            saveSettings();
            updateUi();
        });
        connect(slideDuration, &QSpinBox::valueChanged, this, [this] { saveSettings(); });
        connect(toggleButton, &QPushButton::clicked, this, &ToggleWindow::toggleTarget);
        connect(hideButton, &QPushButton::clicked, this, &QWidget::hide);

        shortcutAction = new QAction(this);
        shortcutAction->setObjectName(QStringLiteral("toggleTarget"));
        shortcutAction->setText(QStringLiteral("Toggle target"));
        KGlobalAccel::self()->setDefaultShortcut(shortcutAction, QList<QKeySequence>{defaultShortcut()});
        registerShortcut();
        connect(shortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, [this] {
            saveSettings();
            registerShortcut();
        });
        connect(shortcutAction, &QAction::triggered, this, &ToggleWindow::toggleTarget);
        updateUi();
        QTimer::singleShot(0, this, [this] { resizeNestedWindow(); });
    }

    ~ToggleWindow() override {
        if (shortcutAction)
            KGlobalAccel::self()->removeAllShortcuts(shortcutAction);
    }

private:
    static QKeySequence defaultShortcut() {
        return QKeySequence(Qt::META | Qt::Key_QuoteLeft);
    }

    QString markerPath() const {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return QDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime).filePath(QStringLiteral("kde-toggle-nested.pid"));
    }

    qint64 nestedWindowPid() const {
        QFile marker(markerPath());
        if (!marker.open(QIODevice::ReadOnly | QIODevice::Text))
            return 0;
        bool ok = false;
        const qint64 pid = marker.readLine().trimmed().toLongLong(&ok);
        return ok && pid > 0 && ::kill(pid, 0) == 0 ? pid : 0;
    }

    void loadSettings() {
        QSettings settings;
        const bool savedPercent = settings.value(QStringLiteral("sizePercent"), false).toBool();
        const auto savedMode = settings.value(QStringLiteral("mode"), QStringLiteral("desktop")).toString();
        const int modeIndex = mode->findData(savedMode);
        if (modeIndex >= 0)
            mode->setCurrentIndex(modeIndex);
        desktop->setValue(settings.value(QStringLiteral("desktop"), 2).toInt());
        command->setText(settings.value(QStringLiteral("command"), QStringLiteral("konsole --new-window")).toString());
        int savedWidth = settings.value(QStringLiteral("nestedWidth"), 1280).toInt();
        int savedHeight = settings.value(QStringLiteral("nestedHeight"), 800).toInt();
        if (savedPercent) {
            savedWidth = qRound(savedWidth * screenWidthMax / 100.0);
            savedHeight = qRound(savedHeight * screenHeightMax / 100.0);
        }
        nestedWidth->setValue(savedWidth);
        nestedHeight->setValue(savedHeight);
        sizePercent->setChecked(savedPercent);
        setSizePercentMode(savedPercent);
        const int edgeIndex = edge->findData(settings.value(QStringLiteral("edge"), QStringLiteral("bottom")));
        if (edgeIndex >= 0)
            edge->setCurrentIndex(edgeIndex);
        animateSlide->setChecked(settings.value(QStringLiteral("animateSlide"), true).toBool());
        slideDuration->setValue(settings.value(QStringLiteral("slideDuration"), 800).toInt());
        const auto savedShortcut = settings.value(QStringLiteral("shortcut")).toString();
        shortcutEdit->setKeySequence(savedShortcut.isEmpty()
                                         ? defaultShortcut()
                                         : QKeySequence::fromString(savedShortcut, QKeySequence::PortableText));
    }

    void saveSettings() {
        QSettings settings;
        settings.setValue(QStringLiteral("mode"), mode->currentData().toString());
        settings.setValue(QStringLiteral("desktop"), desktop->value());
        settings.setValue(QStringLiteral("command"), command->text());
        settings.setValue(QStringLiteral("nestedWidth"), nestedWidth->value());
        settings.setValue(QStringLiteral("nestedHeight"), nestedHeight->value());
        settings.setValue(QStringLiteral("sizePercent"), sizePercent->isChecked());
        settings.setValue(QStringLiteral("edge"), edge->currentData().toString());
        settings.setValue(QStringLiteral("animateSlide"), animateSlide->isChecked());
        settings.setValue(QStringLiteral("slideDuration"), slideDuration->value());
        settings.setValue(QStringLiteral("shortcut"), shortcutEdit->keySequence().toString(QKeySequence::PortableText));
    }

    void registerShortcut() {
        if (!shortcutAction || !shortcutEdit)
            return;
        const auto sequence = shortcutEdit->keySequence();
        if (sequence.isEmpty()) {
            status->setText(QStringLiteral("Choose a shortcut before using the global toggle."));
            return;
        }
        const bool registered = KGlobalAccel::self()->setShortcut(shortcutAction, QList<QKeySequence>{sequence});
        const auto shown = sequence.toString(QKeySequence::NativeText);
        status->setText(registered
                            ? QStringLiteral("Shortcut: %1 | Ready").arg(shown)
                            : QStringLiteral("Shortcut %1 is unavailable or already in use.").arg(shown));
    }

    void updateUi() {
        const auto selectedMode = mode->currentData().toString();
        desktop->setEnabled(selectedMode == QStringLiteral("desktop"));
        command->setEnabled(selectedMode == QStringLiteral("application"));
        const bool nestedMode = selectedMode == QStringLiteral("nested");
        nestedWidth->setEnabled(nestedMode);
        sizePercent->setEnabled(nestedMode);
        nestedWidthSlider->setEnabled(nestedMode);
        nestedHeight->setEnabled(nestedMode);
        nestedHeightSlider->setEnabled(nestedMode);
        edge->setEnabled(nestedMode);
        animateSlide->setEnabled(selectedMode == QStringLiteral("nested"));
        slideDuration->setEnabled(selectedMode == QStringLiteral("nested") && animateSlide->isChecked());
    }

    void setSizePercentMode(bool percent) {
        const int oldWidth = nestedWidth->value();
        const int oldHeight = nestedHeight->value();
        const int widthValue = percent
                                   ? qBound(10, qRound(oldWidth * 100.0 / screenWidthMax), 100)
                                   : qBound(320, qRound(oldWidth * screenWidthMax / 100.0), screenWidthMax);
        const int heightValue = percent
                                    ? qBound(10, qRound(oldHeight * 100.0 / screenHeightMax), 100)
                                    : qBound(200, qRound(oldHeight * screenHeightMax / 100.0), screenHeightMax);
        const QSignalBlocker widthBlock(nestedWidth);
        const QSignalBlocker widthSliderBlock(nestedWidthSlider);
        const QSignalBlocker heightBlock(nestedHeight);
        const QSignalBlocker heightSliderBlock(nestedHeightSlider);
        nestedWidth->setRange(percent ? 10 : 320, percent ? 100 : screenWidthMax);
        nestedHeight->setRange(percent ? 10 : 200, percent ? 100 : screenHeightMax);
        nestedWidthSlider->setRange(percent ? 10 : 320, percent ? 100 : screenWidthMax);
        nestedHeightSlider->setRange(percent ? 10 : 200, percent ? 100 : screenHeightMax);
        nestedWidth->setSingleStep(percent ? 1 : 80);
        nestedHeight->setSingleStep(percent ? 1 : 60);
        nestedWidthSlider->setSingleStep(percent ? 1 : 80);
        nestedHeightSlider->setSingleStep(percent ? 1 : 60);
        nestedWidthSlider->setPageStep(percent ? 5 : 160);
        nestedHeightSlider->setPageStep(percent ? 5 : 120);
        nestedWidth->setSuffix(percent ? QStringLiteral(" %") : QStringLiteral(" px"));
        nestedHeight->setSuffix(percent ? QStringLiteral(" %") : QStringLiteral(" px"));
        nestedWidth->setValue(widthValue);
        nestedHeight->setValue(heightValue);
        nestedWidthSlider->setValue(widthValue);
        nestedHeightSlider->setValue(heightValue);
    }

    int nestedWidthPixels() const {
        return sizePercent->isChecked()
                   ? qRound(nestedWidth->value() * screenWidthMax / 100.0)
                   : nestedWidth->value();
    }

    int nestedHeightPixels() const {
        return sizePercent->isChecked()
                   ? qRound(nestedHeight->value() * screenHeightMax / 100.0)
                   : nestedHeight->value();
    }

    bool kwinDesktop(int *value = nullptr) {
        QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                            QStringLiteral("org.kde.KWin"), QDBusConnection::sessionBus());
        if (!kwin.isValid())
            return false;
        if (value) {
            const QDBusReply<int> reply = kwin.call(QStringLiteral("currentDesktop"));
            if (!reply.isValid())
                return false;
            *value = reply.value();
        }
        return true;
    }

    bool setKwinDesktop(int value) {
        QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
                            QStringLiteral("org.kde.KWin"), QDBusConnection::sessionBus());
        if (!kwin.isValid())
            return false;
        const QDBusReply<void> reply = kwin.call(QStringLiteral("setCurrentDesktop"), value);
        return reply.isValid();
    }

    QString nestedSessionHelper() const {
        const auto appDir = QDir(QCoreApplication::applicationDirPath());
        const auto installed = appDir.filePath(QStringLiteral("../libexec/kde-toggle/nested-plasma-session"));
        if (QFileInfo::exists(installed))
            return QFileInfo(installed).canonicalFilePath();
        return QFileInfo(appDir.filePath(QStringLiteral("../scripts/nested-plasma-session"))).canonicalFilePath();
    }

    QString animationScript(qint64 pid, const QString &action) {
        if (!activePlugin.isEmpty()) {
            QProcess::execute(QStringLiteral("qdbus6"), {
                QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                QStringLiteral("org.kde.kwin.Scripting.unloadScript"), activePlugin
            });
            activePlugin.clear();
        }
        const auto plugin = QStringLiteral("kde-toggle-%1-%2").arg(pid).arg(++scriptCounter);
        const auto path = QDir::temp().filePath(plugin + QStringLiteral(".js"));
        const auto edgeName = edge->currentData().toString();
        const auto animate = animateSlide->isChecked() ? QStringLiteral("true") : QStringLiteral("false");
        const int durationMs = slideDuration->value();
        const auto source = QStringLiteral(R"JS(
var targetPid = %1;
var action = "%2";
var edge = "%3";
var animate = %4;
var duration = %5;
var requestedWidth = %6;
var requestedHeight = %7;
var currentlyVisible = %8;
var found = false;
var timers = [];

function applyToWindow(window) {
    if (window.pid != targetPid || found) return;
    found = true;
    print("kde-toggle: matched nested window pid " + window.pid);
    window.keepAbove = true;
    window.skipTaskbar = true;
    window.skipPager = true;
    window.onAllDesktops = true;
    window.noBorder = true;

    var area = workspace.clientArea(KWin.FullArea, window);
    var width = action === "resize" ? requestedWidth : window.width;
    var height = action === "resize" ? requestedHeight : window.height;
    var targetX = area.x + (area.width - width) / 2;
    var targetY = area.y + (area.height - height) / 2;
    var hiddenX = targetX;
    var hiddenY = targetY;

    if (edge === "bottom") {
        targetY = area.y + area.height - height;
        hiddenY = area.y + area.height + 8;
    } else if (edge === "top") {
        targetY = area.y;
        hiddenY = area.y - height - 8;
    } else if (edge === "left") {
        targetX = area.x;
        hiddenX = area.x - width - 8;
    } else if (edge === "right") {
        targetX = area.x + area.width - width;
        hiddenX = area.x + area.width + 8;
    }

    var currentGeometry = window.frameGeometry;
    var currentX = Math.round(currentGeometry.x);
    var currentY = Math.round(currentGeometry.y);
    var fromX = currentX;
    var fromY = currentY;
    if (action === "show" && window.minimized) {
        fromX = hiddenX;
        fromY = hiddenY;
    }
    var toX = action === "show" ? targetX : action === "hide" ? hiddenX : (currentlyVisible ? targetX : hiddenX);
    var toY = action === "show" ? targetY : action === "hide" ? hiddenY : (currentlyVisible ? targetY : hiddenY);
    fromX = Math.round(fromX);
    fromY = Math.round(fromY);
    toX = Math.round(toX);
    toY = Math.round(toY);

     print("kde-toggle: animation action=" + action + " animate=" + animate
          + " duration=" + duration + " from=" + fromX + "," + fromY
          + " to=" + toX + "," + toY);

    if (!animate || action === "resize") {
        window.frameGeometry = {x: toX, y: toY, width: width, height: height};
        if (action === "show") {
            window.minimized = false;
            workspace.raiseWindow(window);
            workspace.activeWindow = window;
        }
        return;
    }

    window.frameGeometry = {x: fromX, y: fromY, width: width, height: height};

    if (action === "show") {
        window.minimized = false;
        workspace.raiseWindow(window);
        workspace.activeWindow = window;
    }

    var start = Date.now();
    var timer = new QTimer();
    var firstTick = true;
    timer.interval = 16;
    timers.push(timer);
    timer.timeout.connect(function() {
        var progress = Math.min(1, (Date.now() - start) / duration);
        if (firstTick) {
            print("kde-toggle: animation first tick");
            firstTick = false;
        }
        var eased = 1 - Math.pow(1 - progress, 3);
        window.frameGeometry = {x: fromX + (toX - fromX) * eased,
                                y: fromY + (toY - fromY) * eased,
                                width: width, height: height};
        if (progress >= 1) {
            timer.stop();
            print("kde-toggle: animation complete");
            if (action === "show") {
                workspace.raiseWindow(window);
                workspace.activeWindow = window;
            }
        }
    });
    timer.start();
}

workspace.windowList().forEach(applyToWindow);
if (!found) print("kde-toggle: no window matched nested pid " + targetPid);
)JS").arg(pid).arg(action).arg(edgeName).arg(animate).arg(durationMs).arg(nestedWidthPixels()).arg(nestedHeightPixels()).arg(nestedVisible ? QStringLiteral("true") : QStringLiteral("false"));
        QFile script(path);
        if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return {};
        script.write(source.toUtf8());
        script.close();

        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting.loadScript"), path, plugin
        });
        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting.start")
        });
        activePlugin = plugin;
        const int unloadDelay = qMax(1000, slideDuration->value() + 500);
        QTimer::singleShot(unloadDelay, this, [this, plugin, path] {
            QFile::remove(path);
            QProcess::execute(QStringLiteral("qdbus6"), {
                QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                QStringLiteral("org.kde.kwin.Scripting.unloadScript"), plugin
            });
            if (activePlugin == plugin)
                activePlugin.clear();
        });
        return plugin;
    }

    void resizeNestedWindow() {
        if (mode->currentData().toString() != QStringLiteral("nested"))
            return;
        const qint64 pid = nestedWindowPid();
        if (pid > 0)
            animationScript(pid, QStringLiteral("resize"));
    }


    void animateNested(const QString &action) {
        const qint64 pid = nestedWindowPid();
        if (pid <= 0) {
            status->setText(QStringLiteral("Nested Plasma window is not ready yet."));
            return;
        }
        animationScript(pid, action);
        nestedVisible = action == QStringLiteral("show");
    }

    void waitForNestedWindow(int attempts = 0) {
        const qint64 pid = nestedWindowPid();
        if (pid > 0) {
            nestedVisible = false;
            animateNested(QStringLiteral("show"));
            hide();
            return;
        }
        if (attempts < 24) {
            QTimer::singleShot(250, this, [this, attempts] { waitForNestedWindow(attempts + 1); });
        } else {
            status->setText(QStringLiteral("Nested session started but its KWin window was not found."));
        }
    }

    void launchNested() {
        const auto helper = nestedSessionHelper();
        if (!QFileInfo::exists(helper)) {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"),
                                 QStringLiteral("Nested-session helper was not found at %1").arg(helper));
            return;
        }
        const QStringList args{
            QStringLiteral("kwin_wayland"), QStringLiteral("--xwayland"),
            QStringLiteral("--width"), QString::number(nestedWidthPixels()),
            QStringLiteral("--height"), QString::number(nestedHeightPixels()),
            QStringLiteral("--no-global-shortcuts"), QStringLiteral("--no-lockscreen"),
            QStringLiteral("--exit-with-session"), helper
        };
        qint64 pid = 0;
        if (!QProcess::startDetached(QStringLiteral("dbus-run-session"), args, QString(), &pid)) {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"),
                                 QStringLiteral("Could not start the nested Plasma session."));
            return;
        }
        applicationPid = pid;
        nestedVisible = false;
        status->setText(QStringLiteral("Starting nested Plasma desktop&"));
        waitForNestedWindow();
    }

    void toggleNestedDesktop() {
        const qint64 nestedPid = nestedWindowPid();
        if (nestedPid <= 0) {
            launchNested();
            return;
        }
        const bool showing = !nestedVisible;
        animateNested(showing ? QStringLiteral("show") : QStringLiteral("hide"));
        status->setText(showing ? QStringLiteral("Showing nested Plasma desktop")
                                : QStringLiteral("Hiding nested Plasma desktop"));
    }

    void toggleDesktop() {
        int current = 0;
        if (!kwinDesktop(&current)) {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"), QStringLiteral("Could not talk to KWin over D-Bus."));
            return;
        }
        const int target = desktop->value();
        const int destination = current == target ? (previousDesktop > 0 && previousDesktop != target ? previousDesktop : 1) : target;
        if (current != target) previousDesktop = current;
        if (setKwinDesktop(destination)) {
            status->setText(QStringLiteral("Switched from desktop %1 to %2").arg(current).arg(destination));
            hide();
        } else {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"), QStringLiteral("KWin rejected the desktop switch."));
        }
    }

    void toggleApplication() {
        if (applicationPid > 0) {
            if (::kill(applicationPid, SIGTERM) == 0) {
                status->setText(QStringLiteral("Stopped application (PID %1)").arg(applicationPid));
                applicationPid = 0;
                hide();
                return;
            }
            applicationPid = 0;
        }
        const QStringList parts = QProcess::splitCommand(command->text().trimmed());
        if (parts.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"), QStringLiteral("Enter an application command first."));
            return;
        }
        qint64 pid = 0;
        if (!QProcess::startDetached(parts.first(), parts.mid(1), QString(), &pid)) {
            QMessageBox::warning(this, QStringLiteral("KDE Toggle"), QStringLiteral("Could not start: %1").arg(parts.first()));
            return;
        }
        applicationPid = pid;
        status->setText(QStringLiteral("Started %1 (PID %2)").arg(parts.first()).arg(pid));
        hide();
    }

    void toggleTarget() {
        saveSettings();
        const auto selectedMode = mode->currentData().toString();
        if (selectedMode == QStringLiteral("desktop"))
            toggleDesktop();
        else if (selectedMode == QStringLiteral("nested"))
            toggleNestedDesktop();
        else
            toggleApplication();
    }

    QComboBox *mode = nullptr;
    QSpinBox *desktop = nullptr;
    QLineEdit *command = nullptr;
    QSpinBox *nestedWidth = nullptr;
    QSlider *nestedWidthSlider = nullptr;
    QSpinBox *nestedHeight = nullptr;
    QSlider *nestedHeightSlider = nullptr;
    QCheckBox *sizePercent = nullptr;
    QComboBox *edge = nullptr;
    QCheckBox *animateSlide = nullptr;
    QSpinBox *slideDuration = nullptr;
    QKeySequenceEdit *shortcutEdit = nullptr;
    QLabel *status = nullptr;
    QSystemTrayIcon *tray = nullptr;
    QAction *shortcutAction = nullptr;
    int screenWidthMax = 3840;
    int screenHeightMax = 2160;
    int previousDesktop = 0;
    qint64 applicationPid = 0;
    bool nestedVisible = false;
    int scriptCounter = 0;
    QString activePlugin;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-toggle"));
    QApplication::setApplicationDisplayName(QStringLiteral("KDE Toggle"));
    QApplication::setOrganizationName(QStringLiteral("Prototype"));
    ToggleWindow window;
    window.show();
    return app.exec();
}
