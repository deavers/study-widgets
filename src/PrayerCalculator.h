#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QTimeZone>
#include <QtMath>
#include <cmath>

struct PrayerSchedule {
    QTime fajr;
    QTime sunrise;
    QTime dhuhr;
    QTime asr;
    QTime maghrib;
    QTime isha;
    QTime firstThird;
    QTime midnight;
    QTime lastThird;
    QTime duhaStart;
    QTime duhaEnd;
};

struct PrayerConfig
{
    QString presetName = "Standard 12° (UOIF)";
    double latitude = 50.08;
    double longitude = 14.43;
    double elevation = 200.0;
    double fajrAngle = 12.0;    // UOIF 12° or 8° correction
    double ishaAngle = 12.0;    // UOIF 12° or 8° correction
    bool hanafiAsr = false;     // false = Standard (1x shadow), true = Hanafi (2x shadow)

    // Minute tuning offsets
    int tuneFajr = 0;
    int tuneSunrise = 0;
    int tuneDhuhr = 2;
    int tuneAsr = 0;
    int tuneMaghrib = 2;
    int tuneIsha = 0;
};

class PrayerCalculator
{
public:
    // Astronomical prayer time equations (Dr. Monzur Ahmed / UOIF standard)
    static PrayerSchedule calculate(const QDate& date, const PrayerConfig& cfg) {
        int Y = date.year();
        int M = date.month();
        int D = date.day();
        if (M <= 2) {
            Y -= 1;
            M += 12;
        }
        const int A = Y / 100;
        const int B = 2 - A + (A / 4);
        const double JD = std::floor(365.25 * (Y + 4716))
                        + std::floor(30.6001 * (M + 1))
                        + D + B - 1524.5;

        const double d = JD - 2451545.0;

        // Solar coordinates
        const double g = fixAngle(357.529 + 0.98560028 * d);
        const double q = fixAngle(280.459 + 0.98564736 * d);
        const double L = fixAngle(q + 1.915 * dsin(g) + 0.020 * dsin(2.0 * g));

        const double e = 23.439 - 0.00000036 * d;
        const double RA = fixHour(darctan2(dcos(e) * dsin(L), dcos(L)) / 15.0);
        const double decl = darcsin(dsin(e) * dsin(L));
        const double EqT = q / 15.0 - RA;

        const QDateTime noonLocal(date, QTime(12, 0), QTimeZone::systemTimeZone());
        const double tzOffsetHours = noonLocal.offsetFromUtc() / 3600.0;

        // Solar Noon (Dhuhr)
        const double dhuhrH = 12.0 + tzOffsetHours - (cfg.longitude / 15.0) - EqT;

        // Horizon dip for elevation (0.833 + 0.0347 * sqrt(elevation))
        const double horizonAngle = 0.833 + 0.0347 * std::sqrt(qMax(0.0, cfg.elevation));

        const double sunriseH = dhuhrH - hourAngle(horizonAngle, cfg.latitude, decl);
        const double maghribH = dhuhrH + hourAngle(horizonAngle, cfg.latitude, decl);
        const double fajrH    = dhuhrH - hourAngle(cfg.fajrAngle, cfg.latitude, decl);
        const double ishaH    = dhuhrH + hourAngle(cfg.ishaAngle, cfg.latitude, decl);

        const double asrFactor = cfg.hanafiAsr ? 2.0 : 1.0;
        const double asrAlt = -darccot(asrFactor + dtan(std::abs(cfg.latitude - decl)));
        const double asrH = dhuhrH + hourAngle(asrAlt, cfg.latitude, decl);

        PrayerSchedule s;
        s.fajr    = hoursToTime(fajrH, cfg.tuneFajr);
        s.sunrise = hoursToTime(sunriseH, cfg.tuneSunrise);
        s.dhuhr   = hoursToTime(dhuhrH, cfg.tuneDhuhr);
        s.asr     = hoursToTime(asrH, cfg.tuneAsr);
        s.maghrib = hoursToTime(maghribH, cfg.tuneMaghrib);
        s.isha    = hoursToTime(ishaH, cfg.tuneIsha);

        const int magSecs = QTime(0, 0).secsTo(s.maghrib);
        const int fajSecs = QTime(0, 0).secsTo(s.fajr);
        const int nightDuration = (24 * 3600 - magSecs) + fajSecs;

        s.firstThird = s.maghrib.addSecs(nightDuration / 3);
        s.midnight   = s.maghrib.addSecs(nightDuration / 2);
        s.lastThird  = s.maghrib.addSecs((nightDuration * 2) / 3);
        s.duhaStart  = s.sunrise.addSecs(20 * 60);
        s.duhaEnd    = s.dhuhr.addSecs(-15 * 60);

        return s;
    }

private:
    static double dsin(double deg) { return std::sin(qDegreesToRadians(deg)); }
    static double dcos(double deg) { return std::cos(qDegreesToRadians(deg)); }
    static double dtan(double deg) { return std::tan(qDegreesToRadians(deg)); }
    static double darcsin(double x) { return qRadiansToDegrees(std::asin(qBound(-1.0, x, 1.0))); }
    static double darccos(double x) { return qRadiansToDegrees(std::acos(qBound(-1.0, x, 1.0))); }
    static double darctan2(double y, double x) { return qRadiansToDegrees(std::atan2(y, x)); }
    static double darccot(double x) { return qRadiansToDegrees(std::atan(1.0 / x)); }

    static double fixAngle(double a) {
        a = a - 360.0 * std::floor(a / 360.0);
        return a < 0 ? a + 360.0 : a;
    }

    static double fixHour(double h) {
        h = h - 24.0 * std::floor(h / 24.0);
        return h < 0 ? h + 24.0 : h;
    }

    static double hourAngle(double angle, double lat, double decl) {
        double term = (-dsin(angle) - dsin(lat) * dsin(decl)) / (dcos(lat) * dcos(decl));
        term = qBound(-1.0, term, 1.0);
        return darccos(term) / 15.0;
    }

    static QTime hoursToTime(double hours, int tuneMinutes) {
        hours = fixHour(hours + tuneMinutes / 60.0);
        const int totalSeconds = static_cast<int>(std::round(hours * 3600.0)) % (24 * 3600);
        return QTime(0, 0).addSecs(totalSeconds);
    }
};
