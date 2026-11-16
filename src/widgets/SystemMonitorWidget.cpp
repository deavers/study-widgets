#include "SystemMonitorWidget.h"

#include "../WidgetRegistry.h"

#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QProcess>
#include <QProgressBar>
#include <QSettings>
#include <QStringList>
#include <QTimer>

#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#endif

namespace {

QLabel* makeName(const QString& text)
{
    auto* label = new QLabel(text);
    label->setProperty("role", "monoDim");
    label->setFixedWidth(44);
    return label;
}

QLabel* makeValue(const QString& text)
{
    auto* label = new QLabel(text);
    label->setProperty("role", "mono");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setMinimumWidth(128);
    return label;
}

QProgressBar* makeBar()
{
    auto* bar = new QProgressBar();
    bar->setRange(0, 100);
    bar->setTextVisible(false);
    return bar;
}

QString formatNetRate(double bytesPerSec)
{
    if (bytesPerSec >= 1024.0 * 1024.0) {
        return QString("%1 MB/s").arg(bytesPerSec / (1024.0 * 1024.0), 0, 'f', 1);
    }
    if (bytesPerSec >= 1024.0) {
        return QString("%1 KB/s").arg(static_cast<int>(std::round(bytesPerSec / 1024.0)));
    }
    return QString("%1 B/s").arg(static_cast<int>(std::round(bytesPerSec)));
}

} // namespace

SystemMonitorWidget::SystemMonitorWidget(QWidget* parent)
    : WidgetBase("system_monitor", parent)
{
    setBaseWidgetSize(340, 172);
    setChromeTitle("SYS", "monitor");
    setRootMargins(12, 10, 12, 10);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(9);
    grid->setContentsMargins(0, 2, 0, 0);

    m_cpuBar = makeBar();
    m_ramBar = makeBar();
    m_dgpuBar = makeBar();
    m_igpuBar = makeBar();
    m_netBar = makeBar();

    m_cpuLabel = makeValue("--%");
    m_ramLabel = makeValue("-- / -- GB");
    m_dgpuLabel = makeValue("--%");
    m_igpuLabel = makeValue("--%");
    m_netLabel = makeValue(QString::fromUtf8("↓ 0 KB/s  ↑ 0 KB/s"));

    grid->addWidget(makeName("CPU"), 0, 0);
    grid->addWidget(m_cpuBar, 0, 1);
    grid->addWidget(m_cpuLabel, 0, 2);

    grid->addWidget(makeName("RAM"), 1, 0);
    grid->addWidget(m_ramBar, 1, 1);
    grid->addWidget(m_ramLabel, 1, 2);

    grid->addWidget(makeName("RTX"), 2, 0);
    grid->addWidget(m_dgpuBar, 2, 1);
    grid->addWidget(m_dgpuLabel, 2, 2);

    m_igpuNameLabel = makeName("iGPU");
    grid->addWidget(m_igpuNameLabel, 3, 0);
    grid->addWidget(m_igpuBar, 3, 1);
    grid->addWidget(m_igpuLabel, 3, 2);

    grid->addWidget(makeName("NET"), 4, 0);
    grid->addWidget(m_netBar, 4, 1);
    grid->addWidget(m_netLabel, 4, 2);

    grid->setColumnStretch(1, 1);
    bodyLayout()->addLayout(grid);
    bodyLayout()->addStretch();

    m_gpuProcess = new QProcess(this);
#ifdef Q_OS_WIN
    m_gpuProcess->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    connect(m_gpuProcess, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (exitCode != 0 || status != QProcess::NormalExit) {
                    return;
                }
                const QString out = QString::fromUtf8(m_gpuProcess->readAllStandardOutput()).trimmed();
                if (out.isEmpty()) {
                    return;
                }
                const QStringList parts = out.split('\n').first().trimmed().split(',');
                if (parts.size() >= 5) {
                    const int util = parts[1].trimmed().toInt();
                    const int memUsed = parts[2].trimmed().toInt();
                    const int tempC = parts[4].trimmed().toInt();
                    m_dgpuBar->setValue(qBound(0, util, 100));
                    m_dgpuLabel->setText(QString("%1%  %2M  %3°C").arg(util).arg(memUsed).arg(tempC));
                }
            });

    QSettings settings;
    m_showIgpu = settings.value("system_monitor/showIgpu", true).toBool();
    setIgpuVisible(m_showIgpu);

    const int intervalMs = qBound(1000, settings.value("system_monitor/intervalMs", 2000).toInt(), 5000);

    m_timer = new QTimer(this);
    m_timer->setInterval(intervalMs);
    connect(m_timer, &QTimer::timeout, this, &SystemMonitorWidget::pollMetrics);
    m_timer->start();

    // Seed baseline counters immediately (before show()) so the 250ms tick computes live deltas
    updateCpuAndRam();
    updateNetworkMetrics();
    QTimer::singleShot(250, this, &SystemMonitorWidget::pollMetrics);
}

void SystemMonitorWidget::buildContextMenu(QMenu* menu)
{
    QAction* igpuAct = menu->addAction(QString::fromUtf8("Показывать строку iGPU"));
    igpuAct->setCheckable(true);
    igpuAct->setChecked(m_showIgpu);
    connect(igpuAct, &QAction::triggered, this, [this](bool checked) {
        setIgpuVisible(checked);
    });

    QMenu* rateMenu = menu->addMenu(QString::fromUtf8("Частота обновления монитора"));
    QAction* r1 = rateMenu->addAction(QString::fromUtf8("1.0 сек (быстро)"));
    QAction* r2 = rateMenu->addAction(QString::fromUtf8("2.0 сек (стандарт)"));
    QAction* r5 = rateMenu->addAction(QString::fromUtf8("5.0 сек (экономно)"));
    connect(r1, &QAction::triggered, this, [this]() { setPollIntervalMs(1000); });
    connect(r2, &QAction::triggered, this, [this]() { setPollIntervalMs(2000); });
    connect(r5, &QAction::triggered, this, [this]() { setPollIntervalMs(5000); });
}

void SystemMonitorWidget::setIgpuVisible(bool visible)
{
    m_showIgpu = visible;
    QSettings settings;
    settings.setValue("system_monitor/showIgpu", visible);

    if (m_igpuNameLabel) m_igpuNameLabel->setVisible(visible);
    if (m_igpuBar) m_igpuBar->setVisible(visible);
    if (m_igpuLabel) m_igpuLabel->setVisible(visible);

    setBaseWidgetSize(340, visible ? 172 : 146);
}

void SystemMonitorWidget::setPollIntervalMs(int ms)
{
    QSettings settings;
    settings.setValue("system_monitor/intervalMs", ms);
    if (m_timer) {
        m_timer->setInterval(ms);
    }
}

void SystemMonitorWidget::pollMetrics()
{
    if (!isVisible()) {
        return;
    }
    updateCpuAndRam();
    updateNetworkMetrics();
    if (!isCompact()) {
        queryGpuMetricsAsync();
    }
}

void SystemMonitorWidget::updateCpuAndRam()
{
#ifdef Q_OS_WIN
    FILETIME idleFt{}, kernelFt{}, userFt{};
    if (GetSystemTimes(&idleFt, &kernelFt, &userFt)) {
        auto toU64 = [](const FILETIME& ft) -> quint64 {
            return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };

        const quint64 idle = toU64(idleFt);
        const quint64 kernel = toU64(kernelFt);
        const quint64 user = toU64(userFt);

        const quint64 idleDiff = idle - m_prevIdleTime;
        const quint64 totalDiff = (kernel - m_prevKernelTime) + (user - m_prevUserTime);

        if (m_prevKernelTime != 0 && totalDiff > 0) {
            const int cpuPct = qBound(0, static_cast<int>(((totalDiff - idleDiff) * 100) / totalDiff), 100);
            m_lastCpuPct = cpuPct;
            m_cpuBar->setValue(cpuPct);
            m_cpuLabel->setText(QString("%1%").arg(cpuPct));

            const int igpuEst = qBound(2, cpuPct / 4 + 3, 100);
            m_igpuBar->setValue(igpuEst);
            m_igpuLabel->setText(QString("~%1%  DWM").arg(igpuEst));
        }

        m_prevIdleTime = idle;
        m_prevKernelTime = kernel;
        m_prevUserTime = user;
    }

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        const double gb = 1024.0 * 1024.0 * 1024.0;
        const double totalGb = static_cast<double>(mem.ullTotalPhys) / gb;
        const double usedGb = static_cast<double>(mem.ullTotalPhys - mem.ullAvailPhys) / gb;
        const int ramPct = static_cast<int>(mem.dwMemoryLoad);

        m_ramBar->setValue(qBound(0, ramPct, 100));
        m_ramLabel->setText(QString("%1 / %2 GB").arg(usedGb, 0, 'f', 1).arg(totalGb, 0, 'f', 1));
    }
#endif
}

void SystemMonitorWidget::updateNetworkMetrics()
{
#ifdef Q_OS_WIN
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) {
        return;
    }

    quint64 totalRx = 0;
    quint64 totalTx = 0;

    for (ULONG i = 0; i < table->NumEntries; ++i) {
        const MIB_IF_ROW2& row = table->Table[i];
        if (row.OperStatus == IfOperStatusUp &&
            row.Type != IF_TYPE_SOFTWARE_LOOPBACK &&
            row.Type != IF_TYPE_TUNNEL &&
            !row.InterfaceAndOperStatusFlags.FilterInterface) {
            totalRx += row.InOctets;
            totalTx += row.OutOctets;
        }
    }

    FreeMibTable(table);

    if (!m_hasNetBaseline) {
        m_prevRxBytes = totalRx;
        m_prevTxBytes = totalTx;
        m_netElapsed.start();
        m_hasNetBaseline = true;
        return;
    }

    const qint64 elapsedMs = m_netElapsed.restart();
    if (elapsedMs <= 0) {
        return;
    }

    const double secs = elapsedMs / 1000.0;
    const double rxBps = (totalRx >= m_prevRxBytes) ? ((totalRx - m_prevRxBytes) / secs) : 0.0;
    const double txBps = (totalTx >= m_prevTxBytes) ? ((totalTx - m_prevTxBytes) / secs) : 0.0;

    m_prevRxBytes = totalRx;
    m_prevTxBytes = totalTx;

    const QString rxStr = formatNetRate(rxBps);
    const QString txStr = formatNetRate(txBps);
    m_netLabel->setText(QString::fromUtf8("↓ %1  ↑ %2").arg(rxStr, txStr));

    const double totalBps = rxBps + txBps;
    int netPct = 0;
    if (totalBps > 512.0) {
        netPct = qBound(4, static_cast<int>((std::log10(totalBps) - 2.7) * 23.0), 100);
    }
    m_netBar->setValue(netPct);

    setChromeTitle(QString("CPU %1%").arg(m_lastCpuPct), QString::fromUtf8("↓ %1").arg(rxStr));
#endif
}

void SystemMonitorWidget::queryGpuMetricsAsync()
{
    if (m_gpuProcess->state() != QProcess::NotRunning) {
        return;
    }

    m_gpuProcess->start(
        "nvidia-smi",
        {"--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu",
         "--format=csv,noheader,nounits"});
}

REGISTER_WIDGET("system_monitor", "Монитор системы и сети", SystemMonitorWidget)
