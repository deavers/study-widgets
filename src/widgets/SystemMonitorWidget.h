#pragma once

#include "../WidgetBase.h"

#include <QElapsedTimer>

class QLabel;
class QProcess;
class QProgressBar;
class QTimer;

class SystemMonitorWidget : public WidgetBase
{
public:
    explicit SystemMonitorWidget(QWidget* parent = nullptr);

protected:
    void buildContextMenu(QMenu* menu) override;

private:
    void pollMetrics();
    void updateCpuAndRam();
    void updateNetworkMetrics();
    void queryGpuMetricsAsync();
    void setIgpuVisible(bool visible);
    void setPollIntervalMs(int ms);

    QLabel* m_cpuLabel = nullptr;
    QProgressBar* m_cpuBar = nullptr;

    QLabel* m_ramLabel = nullptr;
    QProgressBar* m_ramBar = nullptr;

    QLabel* m_dgpuLabel = nullptr;
    QProgressBar* m_dgpuBar = nullptr;

    QLabel* m_igpuNameLabel = nullptr;
    QLabel* m_igpuLabel = nullptr;
    QProgressBar* m_igpuBar = nullptr;

    QLabel* m_netLabel = nullptr;
    QProgressBar* m_netBar = nullptr;

    QTimer* m_timer = nullptr;
    QProcess* m_gpuProcess = nullptr;
    QElapsedTimer m_netElapsed;

    quint64 m_prevIdleTime = 0;
    quint64 m_prevKernelTime = 0;
    quint64 m_prevUserTime = 0;

    quint64 m_prevRxBytes = 0;
    quint64 m_prevTxBytes = 0;
    bool m_hasNetBaseline = false;
    bool m_showIgpu = true;
    int m_lastCpuPct = 0;
};
