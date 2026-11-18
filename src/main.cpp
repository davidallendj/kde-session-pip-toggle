#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLockFile>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSpinBox>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <KGlobalAccel>
#include <csignal>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t interruptRequested = 0;

void handleInterrupt(int) {
    interruptRequested = 1;
}
}

class ToggleWindow final : public QMainWindow {
public:
    ToggleWindow() {
        setWindowTitle(QStringLiteral("KDE PiP Session"));
        setMinimumWidth(480);
        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);
        auto *intro = new QLabel(QStringLiteral("A Yakuake-like global toggle for a nested Plasma desktop."), central);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        auto *settingsBox = new QGroupBox(QStringLiteral("Nested session"), central);
        auto *form = new QFormLayout(settingsBox);
        const QRect screenArea = QGuiApplication::primaryScreen()
                                     ? QGuiApplication::primaryScreen()->availableGeometry()
                                     : QRect(0, 0, 3840, 2160);
        const int minNestedWidth = 320;
        const int minNestedHeight = 200;
        screenWidthMax = qMax(minNestedWidth, screenArea.width());
        screenHeightMax = qMax(minNestedHeight, screenArea.height());
        nestedWidth = new QSpinBox(settingsBox);
        nestedWidth->setRange(10, 100);
        nestedWidth->setSingleStep(1);
        nestedWidth->setValue(100);
        nestedWidthSlider = new QSlider(Qt::Horizontal, settingsBox);
        nestedWidthSlider->setRange(10, 100);
        nestedWidthSlider->setSingleStep(1);
        nestedWidthSlider->setPageStep(5);
        nestedWidthSlider->setValue(nestedWidth->value());
        auto *widthControls = new QWidget(settingsBox);
        auto *widthLayout = new QHBoxLayout(widthControls);
        widthLayout->setContentsMargins(0, 0, 0, 0);
        widthLayout->addWidget(nestedWidth);
        widthLayout->addWidget(nestedWidthSlider, 1);
        form->addRow(QStringLiteral("Nested width:"), widthControls);

        nestedHeight = new QSpinBox(settingsBox);
        nestedHeight->setRange(10, 100);
        nestedHeight->setSingleStep(1);
        nestedHeight->setValue(70);
        nestedHeightSlider = new QSlider(Qt::Horizontal, settingsBox);
        nestedHeightSlider->setRange(10, 100);
        nestedHeightSlider->setSingleStep(1);
        nestedHeightSlider->setPageStep(5);
        nestedHeightSlider->setValue(nestedHeight->value());
        auto *heightControls = new QWidget(settingsBox);
        auto *heightLayout = new QHBoxLayout(heightControls);
        heightLayout->setContentsMargins(0, 0, 0, 0);
        heightLayout->addWidget(nestedHeight);
        heightLayout->addWidget(nestedHeightSlider, 1);
        form->addRow(QStringLiteral("Nested height:"), heightControls);

        sizePercent = new QCheckBox(QStringLiteral("Use percentage of screen"), settingsBox);
        sizePercent->setChecked(true);
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
        slideDuration->setValue(500);
        form->addRow(QStringLiteral("Animation duration:"), slideDuration);

        shortcutEdit = new QKeySequenceEdit(settingsBox);
        shortcutEdit->setToolTip(QStringLiteral("Press the desired global shortcut here."));
        form->addRow(QStringLiteral("Global shortcut:"), shortcutEdit);
        startOnLogin = new QCheckBox(QStringLiteral("Start on login"), settingsBox);
        form->addRow(QString(), startOnLogin);
        layout->addWidget(settingsBox);

        auto *actions = new QHBoxLayout();
        auto *toggleButton = new QPushButton(QStringLiteral("Toggle nested session"), central);
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
        trayMenu->addAction(QStringLiteral("Toggle nested session"), this, &ToggleWindow::toggleTarget);
        trayMenu->addAction(QStringLiteral("Show settings"), this, [this] { showNormal(); raise(); activateWindow(); });
        trayMenu->addSeparator();
        trayMenu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);
        tray->setContextMenu(trayMenu);
        tray->setToolTip(QStringLiteral("KDE PiP Session"));
        tray->show();

        loadSettings();
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
            setNestedDecorationVisible(decorationVisible, false);
            saveSettings();
            resizeNestedWindow();
        });
        connect(animateSlide, &QCheckBox::toggled, this, [this] {
            slideDuration->setEnabled(animateSlide->isChecked());
            saveSettings();
            resizeNestedWindow();
        });
        connect(slideDuration, &QSpinBox::valueChanged, this, [this] {
            saveSettings();
            resizeNestedWindow();
        });
        connect(toggleButton, &QPushButton::clicked, this, &ToggleWindow::toggleTarget);
        connect(hideButton, &QPushButton::clicked, this, &QWidget::showMinimized);
        connect(startOnLogin, &QCheckBox::toggled, this, [this](bool checked) {
            updateAutostart(checked);
            saveSettings();
        });

        shortcutAction = new QAction(this);
        shortcutAction->setObjectName(QStringLiteral("toggleTarget"));
        shortcutAction->setText(QStringLiteral("Toggle nested session"));
        KGlobalAccel::self()->setDefaultShortcut(shortcutAction, QList<QKeySequence>{defaultShortcut()});
        decorationAction = new QAction(this);
        decorationAction->setObjectName(QStringLiteral("toggleDecoration"));
        decorationAction->setText(QStringLiteral("Toggle decoration"));
        KGlobalAccel::self()->setDefaultShortcut(decorationAction, QList<QKeySequence>{defaultTitleBarShortcut()});
        registerShortcut();
        registerDecorationShortcut();
        connect(decorationAction, &QAction::triggered, this, [this] { setNestedDecorationVisible(!decorationVisible); });
        connect(shortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, [this] {
            saveSettings();
            registerShortcut();
        });
        connect(shortcutAction, &QAction::triggered, this, &ToggleWindow::toggleTarget);
        decorationCommandPoller.setInterval(100);
        connect(&decorationCommandPoller, &QTimer::timeout, this, &ToggleWindow::pollDecorationCommands);
        decorationCommandPoller.start();
        slideDuration->setEnabled(animateSlide->isChecked());
        QTimer::singleShot(0, this, [this] {
            cleanupPreviousNestedSessions();
            initializeNestedSession();
        });
    }

    ~ToggleWindow() override {
        saveSettings();
        stopNestedSession();
        if (shortcutAction)
            KGlobalAccel::self()->removeAllShortcuts(shortcutAction);
        if (decorationAction)
            KGlobalAccel::self()->removeAllShortcuts(decorationAction);
        QFile::remove(decorationStatePath());
    }

private:
    static QKeySequence defaultShortcut() {
        return QKeySequence(Qt::META | Qt::Key_QuoteLeft);
    }

    static QKeySequence defaultTitleBarShortcut() {
        return QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_T);
    }

    QString markerPath() const {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return QDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime).filePath(QStringLiteral("kde-pip-session-nested.pid"));
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
        if (!settings.contains(QStringLiteral("nestedWidth"))) {
            QSettings legacy(QStringLiteral("Prototype"), QStringLiteral("kde-toggle"));
            const auto legacyKeys = legacy.allKeys();
            for (const auto &key : legacyKeys)
                settings.setValue(key, legacy.value(key));
            settings.sync();
        }
        const bool savedPercent = settings.value(QStringLiteral("sizePercent"), true).toBool();
        const int savedWidthRaw = settings.value(QStringLiteral("nestedWidth"), 100).toInt();
        const int savedHeightRaw = settings.value(QStringLiteral("nestedHeight"), 70).toInt();
        const bool repairLegacyMinimumPercentages = !settings.contains(QStringLiteral("settingsVersion"))
                                                 && savedPercent && savedWidthRaw == 10 && savedHeightRaw == 10;
        const int savedWidth = savedPercent
                                   ? (repairLegacyMinimumPercentages ? 100 : (savedWidthRaw > 100 ? 100 : savedWidthRaw))
                                   : savedWidthRaw;
        const int savedHeight = savedPercent
                                    ? (repairLegacyMinimumPercentages ? 70 : (savedHeightRaw > 100 ? 70 : savedHeightRaw))
                                    : savedHeightRaw;
        nestedWidth->setValue(savedPercent ? qBound(10, savedWidth, 100) : qBound(320, savedWidth, screenWidthMax));
        nestedHeight->setValue(savedPercent ? qBound(10, savedHeight, 100) : qBound(200, savedHeight, screenHeightMax));
        sizePercent->setChecked(savedPercent);
        setSizePercentMode(savedPercent, false);
        const int edgeIndex = edge->findData(settings.value(QStringLiteral("edge"), QStringLiteral("bottom")));
        if (edgeIndex >= 0)
            edge->setCurrentIndex(edgeIndex);
        animateSlide->setChecked(settings.value(QStringLiteral("animateSlide"), true).toBool());
        slideDuration->setValue(settings.value(QStringLiteral("slideDuration"), 500).toInt());
        decorationVisible = settings.value(QStringLiteral("decorationVisible"), true).toBool();
        nestedPinned = settings.value(QStringLiteral("nestedPinned"), false).toBool();
        setNestedDecorationVisible(decorationVisible, false);
        startOnLogin->setChecked(settings.value(QStringLiteral("startOnLogin"), false).toBool());
        updateAutostart(startOnLogin->isChecked());
        const auto savedShortcut = settings.value(QStringLiteral("shortcut")).toString();
        shortcutEdit->setKeySequence(savedShortcut.isEmpty()
                                         ? defaultShortcut()
                                         : QKeySequence::fromString(savedShortcut, QKeySequence::PortableText));
    }

    void saveSettings() {
        QSettings settings;
        settings.setValue(QStringLiteral("nestedWidth"), nestedWidth->value());
        settings.setValue(QStringLiteral("nestedHeight"), nestedHeight->value());
        settings.setValue(QStringLiteral("sizePercent"), sizePercent->isChecked());
        settings.setValue(QStringLiteral("edge"), edge->currentData().toString());
        settings.setValue(QStringLiteral("animateSlide"), animateSlide->isChecked());
        settings.setValue(QStringLiteral("slideDuration"), slideDuration->value());
        settings.setValue(QStringLiteral("shortcut"), shortcutEdit->keySequence().toString(QKeySequence::PortableText));
        settings.setValue(QStringLiteral("decorationVisible"), decorationVisible);
        settings.setValue(QStringLiteral("nestedPinned"), nestedPinned);
        settings.setValue(QStringLiteral("startOnLogin"), startOnLogin->isChecked());
        settings.setValue(QStringLiteral("settingsVersion"), 1);
        settings.remove(QStringLiteral("mode"));
        settings.remove(QStringLiteral("desktop"));
        settings.remove(QStringLiteral("command"));
        settings.remove(QStringLiteral("showTitleBar"));
    }

    QString autostartPath() const {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .filePath(QStringLiteral("autostart/kde-pip-session.desktop"));
    }

    void updateAutostart(bool enabled) {
        const auto path = autostartPath();
        if (!enabled) {
            QFile::remove(path);
            return;
        }
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return;
        const auto executable = QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
        file.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=KDE PiP Session\nExec=\"%1\"\nOnlyShowIn=KDE;\nX-KDE-autostart-phase=2\n").arg(executable).toUtf8());
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

    void registerDecorationShortcut() {
        if (!decorationAction) return;
        const auto sequence = defaultTitleBarShortcut();
        const bool registered = KGlobalAccel::self()->setShortcut(decorationAction, QList<QKeySequence>{sequence});
        if (!registered) status->setText(QStringLiteral("Decoration shortcut %1 is unavailable or already in use.").arg(sequence.toString(QKeySequence::NativeText)));
    }

    void setSizePercentMode(bool percent, bool convertValues = true) {
        const int oldWidth = nestedWidth->value();
        const int oldHeight = nestedHeight->value();
        const int widthValue = !convertValues
                                   ? qBound(percent ? 10 : 320, oldWidth, percent ? 100 : screenWidthMax)
                                   : percent
                                         ? qBound(10, qRound(oldWidth * 100.0 / screenWidthMax), 100)
                                         : qBound(320, qRound(oldWidth * screenWidthMax / 100.0), screenWidthMax);
        const int heightValue = !convertValues
                                    ? qBound(percent ? 10 : 200, oldHeight, percent ? 100 : screenHeightMax)
                                    : percent
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

    QString nestedSessionHelper() const {
        const auto appDir = QDir(QCoreApplication::applicationDirPath());
        const auto installed = appDir.filePath(QStringLiteral("../libexec/kde-pip-session/nested-plasma-session"));
        if (QFileInfo::exists(installed))
            return QFileInfo(installed).canonicalFilePath();
        return QFileInfo(appDir.filePath(QStringLiteral("../scripts/nested-plasma-session"))).canonicalFilePath();
    }

    void cleanupNestedRuntimeFiles() {
        QFile::remove(markerPath());
        QFile::remove(decorationCommandPath());
        QFile::remove(decorationStatePath());
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        QDir runtimeDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime);
        const auto temporaryDirs = runtimeDir.entryList(QStringList{QStringLiteral("kde-pip-session-nested-data-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &name : temporaryDirs)
            QDir(runtimeDir.filePath(name)).removeRecursively();
    }

    void stopNestedSession() {
        if (nestedProcess && nestedProcess->state() != QProcess::NotRunning) {
            nestedProcess->terminate();
            if (!nestedProcess->waitForFinished(3000)) {
                nestedProcess->kill();
                nestedProcess->waitForFinished(1000);
            }
        }
        // QProcess only owns the dbus-run-session root. Clean up any nested
        // compositor/helper descendants that survived its shutdown as well.
        cleanupPreviousNestedSessions();
        nestedVisible = false;
        cleanupNestedRuntimeFiles();
    }

    QString animationScript(qint64 pid, const QString &action) {
        if (!activePlugin.isEmpty()) {
            QProcess::execute(QStringLiteral("qdbus6"), {
                QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"),
                QStringLiteral("org.kde.kwin.Scripting.unloadScript"), activePlugin
            });
            activePlugin.clear();
        }
        const auto plugin = QStringLiteral("kde-pip-session-%1-%2").arg(pid).arg(++scriptCounter);
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
var pinned = %9;
var found = false;
var timers = [];
var targetWindow = null;
var targetArea = null;

function applyToWindow(window) {
    if (window.pid != targetPid || found) return;
    var resolvedAction = action === "hide-instant" ? "hide" : action;
    var useAnimation = animate && action !== "hide-instant";
    found = true;
    targetWindow = window;
    print("kde-pip-session: matched nested window pid " + window.pid);
    window.keepAbove = true;
    window.skipTaskbar = true;
    window.skipPager = true;
    window.onAllDesktops = true;
    window.noBorder = true;

    var area = workspace.clientArea(KWin.FullArea, window);
    targetArea = area;
    var width = resolvedAction === "resize" ? requestedWidth : window.width;
    var height = resolvedAction === "resize" ? requestedHeight : window.height;
    var targetX = area.x + (area.width - width) / 2;
    var targetY = area.y + (area.height - height) / 2;
    var hiddenX = targetX;
    var hiddenY = targetY;
    var hiddenMargin = 64;

    if (edge === "bottom") {
        targetY = area.y + area.height - height;
        hiddenY = area.y + area.height + hiddenMargin;
    } else if (edge === "top") {
        targetY = area.y;
        hiddenY = area.y - height - hiddenMargin;
    } else if (edge === "left") {
        targetX = area.x;
        hiddenX = area.x - width - hiddenMargin;
    } else if (edge === "right") {
        targetX = area.x + area.width - width;
        hiddenX = area.x + area.width + hiddenMargin;
    }

    var currentGeometry = window.frameGeometry;
    var currentX = Math.round(currentGeometry.x);
    var currentY = Math.round(currentGeometry.y);
    var currentlyHidden = currentX + width <= area.x || currentX >= area.x + area.width
                       || currentY + height <= area.y || currentY >= area.y + area.height;
    if (action === "toggle")
        resolvedAction = currentlyHidden ? "show" : "hide";
    var fromX = currentlyHidden ? hiddenX : currentX;
    var fromY = currentlyHidden ? hiddenY : currentY;
    var visibleForPlacement = !currentlyHidden;
    var toX = resolvedAction === "show" ? targetX : resolvedAction === "hide" ? hiddenX : (visibleForPlacement ? targetX : hiddenX);
    var toY = resolvedAction === "show" ? targetY : resolvedAction === "hide" ? hiddenY : (visibleForPlacement ? targetY : hiddenY);
    fromX = Math.round(fromX);
    fromY = Math.round(fromY);
    toX = Math.round(toX);
    toY = Math.round(toY);

     print("kde-pip-session: animation action=" + action + " animate=" + animate
          + " duration=" + duration + " from=" + fromX + "," + fromY
          + " to=" + toX + "," + toY);

    if (!useAnimation || resolvedAction === "resize") {
        window.frameGeometry = {x: toX, y: toY, width: width, height: height};
        if (resolvedAction === "show") {
            workspace.raiseWindow(window);
            workspace.activeWindow = window;
        }
        return;
    }

    window.frameGeometry = {x: fromX, y: fromY, width: width, height: height};

    if (resolvedAction === "show") {
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
            print("kde-pip-session: animation first tick");
            firstTick = false;
        }
        var eased = 1 - Math.pow(1 - progress, 3);
        window.frameGeometry = {x: fromX + (toX - fromX) * eased,
                                y: fromY + (toY - fromY) * eased,
                                width: width, height: height};
        if (progress >= 1) {
            timer.stop();
            // Reapply the exact endpoint after the last interpolated frame. This
            // prevents compositor/frame rounding from leaving a visible sliver.
            window.frameGeometry = {x: toX, y: toY, width: width, height: height};
            print("kde-pip-session: animation complete");
            if (resolvedAction === "show") {
                workspace.raiseWindow(window);
                workspace.activeWindow = window;
            }
        }
    });
    timer.start();
}

function dismissWindow(candidate) {
    if (!candidate) return;
    // The Window wrapper received by windowActivated can no longer be passed
    // back to clientArea on some Plasma/KWin builds. Reuse the area captured
    // while matching the target instead of throwing from the dismissal path.
    var area = targetArea;
    if (!area) return;
    var width = Math.ceil(candidate.width);
    var height = Math.ceil(candidate.height);
    var hiddenMargin = 96;
    var hiddenX = area.x - width - hiddenMargin;
    var hiddenY = area.y - height - hiddenMargin;
    if (edge === "bottom") hiddenY = area.y + area.height + hiddenMargin;
    else if (edge === "top") hiddenY = area.y - height - hiddenMargin;
    else if (edge === "right") hiddenX = area.x + area.width + hiddenMargin;
    else if (edge === "left") hiddenX = area.x - width - hiddenMargin;
    var current = candidate.frameGeometry;
    var fromX = Math.round(current.x);
    var fromY = Math.round(current.y);
    if (fromX + width <= area.x || fromX >= area.x + area.width
        || fromY + height <= area.y || fromY >= area.y + area.height)
        return;

    // Outside activation is a dismissal, not a user-requested slide. Move the
    // mapped window fully off-screen in one operation so unloading a later KWin
    // script can never strand it at an intermediate coordinate.
    candidate.frameGeometry = {x: hiddenX, y: hiddenY, width: width, height: height};
}

workspace.windowList().forEach(applyToWindow);
if (found && !pinned) {
    workspace.windowActivated.connect(function(active) {
        if (active && active !== targetWindow)
            dismissWindow(targetWindow);
    });
}
if (!found) print("kde-pip-session: no window matched nested pid " + targetPid);
)JS").arg(pid).arg(action).arg(edgeName).arg(animate).arg(durationMs).arg(nestedWidthPixels()).arg(nestedHeightPixels()).arg(nestedVisible ? QStringLiteral("true") : QStringLiteral("false")).arg(nestedPinned ? QStringLiteral("true") : QStringLiteral("false"));
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
        return plugin;
    }

    void resizeNestedDimension(const QString &axis, int pixels) {
        const bool widthAxis = axis == QStringLiteral("width");
        const int minimum = widthAxis ? 320 : 200;
        const int maximum = widthAxis ? screenWidthMax : screenHeightMax;
        pixels = qBound(minimum, pixels, maximum);
        if (sizePercent->isChecked()) {
            const int percent = qBound(10, qRound(pixels * 100.0 / maximum), 100);
            if (widthAxis)
                nestedWidth->setValue(percent);
            else
                nestedHeight->setValue(percent);
        } else if (widthAxis) {
            nestedWidth->setValue(pixels);
        } else {
            nestedHeight->setValue(pixels);
        }
    }

    void resizeNestedWindow() {
        if (toggleInFlight)
            return;
        const qint64 pid = nestedWindowPid();
        if (pid > 0)
            animationScript(pid, QStringLiteral("resize"));
    }


    void animateNested(const QString &action) {
        if (toggleInFlight)
            return;
        const qint64 pid = nestedWindowPid();
        if (pid <= 0) {
            status->setText(QStringLiteral("Nested Plasma window is not ready yet."));
            return;
        }
        animationScript(pid, action);
        if (action == QStringLiteral("show"))
            nestedVisible = true;
        else if (action == QStringLiteral("hide"))
            nestedVisible = false;
        else if (action == QStringLiteral("toggle"))
            nestedVisible = !nestedVisible;
        if (action == QStringLiteral("toggle") && animateSlide->isChecked()) {
            toggleInFlight = true;
            QTimer::singleShot(slideDuration->value() + 100, this, [this] {
                toggleInFlight = false;
            });
        }
    }

    void waitForNestedWindow(int attempts = 0) {
        const qint64 pid = nestedWindowPid();
        if (pid > 0) {
            nestedVisible = false;
            animationScript(pid, QStringLiteral("hide-instant"));
            QTimer::singleShot(0, this, [this] { resizeNestedWindow(); });
            return;
        }
        if (attempts < 24) {
            QTimer::singleShot(250, this, [this, attempts] { waitForNestedWindow(attempts + 1); });
        } else {
            status->setText(QStringLiteral("Nested session started but its KWin window was not found."));
        }
    }

    void initializeNestedSession() {
        const qint64 pid = nestedWindowPid();
        if (pid > 0) {
            nestedVisible = false;
            animationScript(pid, QStringLiteral("hide-instant"));
            QTimer::singleShot(0, this, [this] { resizeNestedWindow(); });
        } else {
            launchNested();
        }
    }

    QString nestedDecorationHelper() const {
        const auto appDir = QDir(QCoreApplication::applicationDirPath());
        const auto installed = appDir.filePath(QStringLiteral("../libexec/kde-pip-session/kde-pip-session-decoration"));
        if (QFileInfo::exists(installed))
            return QFileInfo(installed).canonicalFilePath();
        return QFileInfo(appDir.filePath(QStringLiteral("kde-pip-session-decoration"))).canonicalFilePath();
    }

    QString decorationCommandPath() const {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return QDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime).filePath(QStringLiteral("kde-pip-session-decoration.command"));
    }

    QString decorationStatePath() const {
        const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        return QDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime).filePath(QStringLiteral("kde-pip-session-decoration.state"));
    }

    void setNestedDecorationVisible(bool visible, bool persist = true) {
        decorationVisible = visible;
        QSaveFile file(decorationStatePath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write((visible ? QStringLiteral("1\n") : QStringLiteral("0\n"))
                           .append(edge->currentData().toString()).toUtf8());
            file.commit();
        }
        if (persist)
            saveSettings();
    }

    void sendDecorationCommand(const QString &command) {
        QFile file(decorationCommandPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            file.write((command + QLatin1Char('\n')).toUtf8());
    }

    void pollDecorationCommands() {
        QFile file(decorationCommandPath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        const auto commands = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        file.close();
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.close();
        for (const auto &command : commands) {
            if (command == QStringLiteral("toggle"))
                toggleNestedDesktop();
            else if (command == QStringLiteral("settings"))
                showNormal(), raise(), activateWindow();
            else if (command == QStringLiteral("pin 1") || command == QStringLiteral("pin 0")) {
                nestedPinned = command.endsWith(QLatin1Char('1'));
                saveSettings();
                const qint64 pid = nestedWindowPid();
                if (pid > 0)
                    animationScript(pid, QStringLiteral("state"));
            } else if (command == QStringLiteral("hide-decoration")) {
                setNestedDecorationVisible(false);
            } else if (command.startsWith(QStringLiteral("resize "))) {
                const auto parts = command.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (parts.size() == 3) {
                    bool ok = false;
                    const int pixels = parts.at(2).toInt(&ok);
                    if (ok && (parts.at(1) == QStringLiteral("width") || parts.at(1) == QStringLiteral("height")))
                        resizeNestedDimension(parts.at(1), pixels);
                }
            }
        }
    }

    bool nestedSessionProcessExists(const QString &helper) const {
        QProcess probe;
        probe.start(QStringLiteral("pgrep"), {QStringLiteral("-f"), helper});
        if (!probe.waitForFinished(500)) {
            probe.kill();
            probe.waitForFinished(200);
            return false;
        }
        return probe.exitCode() == 0;
    }

    void cleanupPreviousNestedSessions() {
        const auto helper = nestedSessionHelper();
        if (helper.isEmpty() || !nestedSessionProcessExists(helper)) {
            cleanupNestedRuntimeFiles();
            return;
        }

        // A prior controller can die before its dbus-run-session child chain.
        // The helper path is unique to this application, so limit termination
        // to its nested compositor/session processes.
        QProcess::execute(QStringLiteral("pkill"), {
            QStringLiteral("-TERM"), QStringLiteral("-f"), helper
        });
        QElapsedTimer timeout;
        timeout.start();
        while (nestedSessionProcessExists(helper) && timeout.elapsed() < 3000)
            QThread::msleep(100);
        if (nestedSessionProcessExists(helper)) {
            QProcess::execute(QStringLiteral("pkill"), {
                QStringLiteral("-KILL"), QStringLiteral("-f"), helper
            });
        }
        cleanupNestedRuntimeFiles();
    }

    void launchNested() {
        const auto helper = nestedSessionHelper();
        if (!QFileInfo::exists(helper)) {
            QMessageBox::warning(this, QStringLiteral("KDE PiP Session"),
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
        if (nestedProcess && nestedProcess->state() != QProcess::NotRunning) {
            status->setText(QStringLiteral("Nested Plasma desktop is already starting."));
            return;
        }
        if (!nestedProcess)
            nestedProcess = new QProcess(this);
        cleanupNestedRuntimeFiles();
        setNestedDecorationVisible(decorationVisible);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("KDE_PIP_SESSION_DECORATION_HELPER"), nestedDecorationHelper());
        environment.insert(QStringLiteral("KDE_PIP_SESSION_DECORATION_VISIBLE"), decorationVisible ? QStringLiteral("1") : QStringLiteral("0"));
        environment.insert(QStringLiteral("KDE_PIP_SESSION_PINNED"), nestedPinned ? QStringLiteral("1") : QStringLiteral("0"));
        environment.insert(QStringLiteral("KDE_PIP_SESSION_NESTED_VISIBLE"), nestedVisible ? QStringLiteral("1") : QStringLiteral("0"));
        environment.insert(QStringLiteral("KDE_PIP_SESSION_EDGE"), edge->currentData().toString());
        nestedProcess->setProcessEnvironment(environment);
        nestedProcess->start(QStringLiteral("dbus-run-session"), args);
        if (!nestedProcess->waitForStarted(2000)) {
            QMessageBox::warning(this, QStringLiteral("KDE PiP Session"),
                                 QStringLiteral("Could not start the nested Plasma session."));
            return;
        }
        nestedVisible = false;
        status->setText(QStringLiteral("Starting nested Plasma desktop..."));
        waitForNestedWindow();
    }

    void toggleNestedDesktop() {
        if (toggleInFlight) {
            status->setText(QStringLiteral("Toggle ignored while the slide animation is running."));
            return;
        }
        const qint64 nestedPid = nestedWindowPid();
        if (nestedPid <= 0) {
            launchNested();
            return;
        }
        animateNested(QStringLiteral("toggle"));
        status->setText(QStringLiteral("Toggling nested Plasma desktop"));
    }

    void toggleTarget() {
        saveSettings();
        toggleNestedDesktop();
    }

    QSpinBox *nestedWidth = nullptr;
    QSlider *nestedWidthSlider = nullptr;
    QSpinBox *nestedHeight = nullptr;
    QSlider *nestedHeightSlider = nullptr;
    QCheckBox *sizePercent = nullptr;
    QComboBox *edge = nullptr;
    QCheckBox *animateSlide = nullptr;
    QSpinBox *slideDuration = nullptr;
    QKeySequenceEdit *shortcutEdit = nullptr;
    QCheckBox *startOnLogin = nullptr;
    QLabel *status = nullptr;
    QSystemTrayIcon *tray = nullptr;
    QAction *shortcutAction = nullptr;
    QAction *decorationAction = nullptr;
    QTimer decorationCommandPoller;
    QProcess *nestedProcess = nullptr;
    int screenWidthMax = 3840;
    int screenHeightMax = 2160;
    bool nestedVisible = false;
    bool decorationVisible = true;
    bool nestedPinned = false;
    bool toggleInFlight = false;
    int scriptCounter = 0;
    QString activePlugin;
};

int main(int argc, char **argv) {
    std::signal(SIGINT, handleInterrupt);
    std::signal(SIGTERM, handleInterrupt);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("kde-pip-session"));
    QApplication::setApplicationDisplayName(QStringLiteral("KDE PiP Session"));
    QApplication::setOrganizationName(QStringLiteral("Prototype"));
    const auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    QLockFile instanceLock(QDir(runtime.isEmpty() ? QStringLiteral("/tmp") : runtime)
                               .filePath(QStringLiteral("kde-pip-session.lock")));
    if (!instanceLock.tryLock(250)) {
        qWarning("KDE PiP Session is already running for this user.");
        return 0;
    }
    ToggleWindow window;
    window.show();
    QTimer signalPoller;
    QObject::connect(&signalPoller, &QTimer::timeout, &app, [&app] {
        if (interruptRequested != 0)
            app.quit();
    });
    signalPoller.start(100);
    return app.exec();
}
