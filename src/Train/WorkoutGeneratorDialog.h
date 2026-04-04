/*
 * Copyright (c) 2026 GoldenCheetah Development Team
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _WorkoutGeneratorDialog_h
#define _WorkoutGeneratorDialog_h

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>

#include "Context.h"
#include "ErgFile.h"

class WorkoutGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    WorkoutGeneratorDialog(Context *context, QWidget *parent = nullptr);
    ~WorkoutGeneratorDialog();

private slots:
    void onGenerate();
    void onSave();

private:
    void setupUI();
    void loadAthleteData();
    void updatePreview();
    QString generateFilename();

    // workout generators
    void generateRecovery(double available);
    void generateEndurance(double available);
    void generateSweetSpot(double available);
    void generateThreshold(double available);
    void generateVO2max(double available);
    void generateAnaerobic(double available);
    void generateMixed(double available);

    void addWarmup();
    void addCooldown(double startTime);
    void addPoint(double minutes, double watts);
    void addLap(double minutes, const QString &name);
    void addText(double minutes, int durationSecs, const QString &text);
    int scaledIntervalCount(int baseCount, double cycleMinutes, double availableMinutes);

    Context *context;

    // UI
    QLabel *ftpLabel, *ctlLabel, *atlLabel, *tsbLabel;
    QComboBox *workoutTypeCombo;
    QSpinBox *durationSpin;
    QPushButton *generateBtn, *saveBtn;
    QwtPlot *previewPlot;
    QwtPlotCurve *powerCurve;

    // state
    ErgFile *generatedWorkout;
    int athleteFTP;
    double athleteCTL, athleteATL, athleteTSB;

    static constexpr double WARMUP_MINUTES = 10.0;
    static constexpr double COOLDOWN_MINUTES = 5.0;
};

#endif
