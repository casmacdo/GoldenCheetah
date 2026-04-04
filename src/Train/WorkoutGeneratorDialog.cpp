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

#include "WorkoutGeneratorDialog.h"
#include "Athlete.h"
#include "Zones.h"
#include "PMCData.h"
#include "TrainDB.h"
#include "Settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFile>
#include <QDate>

#include <qwt_plot_grid.h>
#include <qwt_axis_id.h>

#include <cmath>
#include <algorithm>

extern TrainDB *trainDB;

WorkoutGeneratorDialog::WorkoutGeneratorDialog(Context *context, QWidget *parent)
    : QDialog(parent), context(context), generatedWorkout(nullptr),
      athleteFTP(0), athleteCTL(0), athleteATL(0), athleteTSB(0)
{
    setWindowTitle(tr("Workout Generator"));
    setMinimumSize(600, 500);
    setupUI();
    loadAthleteData();
}

WorkoutGeneratorDialog::~WorkoutGeneratorDialog()
{
    delete generatedWorkout;
}

void WorkoutGeneratorDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Athlete fitness info
    QGroupBox *fitnessBox = new QGroupBox(tr("Athlete Fitness"));
    QFormLayout *fitnessLayout = new QFormLayout(fitnessBox);
    ftpLabel = new QLabel(tr("N/A"));
    ctlLabel = new QLabel(tr("N/A"));
    atlLabel = new QLabel(tr("N/A"));
    tsbLabel = new QLabel(tr("N/A"));
    fitnessLayout->addRow(tr("FTP:"), ftpLabel);
    fitnessLayout->addRow(tr("CTL (Fitness):"), ctlLabel);
    fitnessLayout->addRow(tr("ATL (Fatigue):"), atlLabel);
    fitnessLayout->addRow(tr("TSB (Form):"), tsbLabel);
    mainLayout->addWidget(fitnessBox);

    // Workout parameters
    QGroupBox *paramBox = new QGroupBox(tr("Workout Parameters"));
    QFormLayout *paramLayout = new QFormLayout(paramBox);

    workoutTypeCombo = new QComboBox();
    workoutTypeCombo->addItem(tr("Recovery"), 0);
    workoutTypeCombo->addItem(tr("Endurance"), 1);
    workoutTypeCombo->addItem(tr("Sweet Spot"), 2);
    workoutTypeCombo->addItem(tr("Threshold"), 3);
    workoutTypeCombo->addItem(tr("VO2max"), 4);
    workoutTypeCombo->addItem(tr("Anaerobic"), 5);
    workoutTypeCombo->addItem(tr("Mixed"), 6);
    paramLayout->addRow(tr("Type:"), workoutTypeCombo);

    durationSpin = new QSpinBox();
    durationSpin->setRange(30, 120);
    durationSpin->setValue(60);
    durationSpin->setSuffix(tr(" min"));
    durationSpin->setSingleStep(5);
    paramLayout->addRow(tr("Duration:"), durationSpin);

    mainLayout->addWidget(paramBox);

    // Generate button
    generateBtn = new QPushButton(tr("Generate Workout"));
    connect(generateBtn, SIGNAL(clicked()), this, SLOT(onGenerate()));
    mainLayout->addWidget(generateBtn);

    // Preview chart
    previewPlot = new QwtPlot();
    previewPlot->setTitle(tr("Workout Preview"));
    previewPlot->setAxisTitle(QwtAxis::XBottom, tr("Time (minutes)"));
    previewPlot->setAxisTitle(QwtAxis::YLeft, tr("Power (watts)"));
    previewPlot->setMinimumHeight(200);

    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->enableX(true);
    grid->enableY(true);
    grid->setMajorPen(QPen(QColor(200, 200, 200), 0, Qt::DotLine));
    grid->attach(previewPlot);

    powerCurve = new QwtPlotCurve(tr("Power"));
    powerCurve->setPen(QPen(Qt::blue, 2));
    powerCurve->setBrush(QBrush(QColor(0, 0, 255, 40)));
    powerCurve->attach(previewPlot);

    mainLayout->addWidget(previewPlot, 1);

    // Bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    saveBtn = new QPushButton(tr("Save Workout"));
    saveBtn->setEnabled(false);
    connect(saveBtn, SIGNAL(clicked()), this, SLOT(onSave()));
    QPushButton *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(close()));
    btnLayout->addStretch();
    btnLayout->addWidget(saveBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
}

void WorkoutGeneratorDialog::loadAthleteData()
{
    // Get FTP
    const Zones *zones = context->athlete->zones("Bike");
    if (zones) {
        int zoneRange = zones->whichRange(QDate::currentDate());
        if (zoneRange >= 0) {
            athleteFTP = zones->getFTP(zoneRange);
            if (athleteFTP == 0) athleteFTP = zones->getCP(zoneRange);
        }
    }

    // Get PMC data
    PMCData *pmc = context->athlete->getPMCFor("coggan_tss");
    if (pmc) {
        QDate today = QDate::currentDate();
        athleteCTL = pmc->lts(today);
        athleteATL = pmc->sts(today);
        athleteTSB = pmc->sb(today);
    }

    // Update labels
    if (athleteFTP > 0) {
        ftpLabel->setText(QString("%1 W").arg(athleteFTP));
    } else {
        ftpLabel->setText(tr("Not set - please configure power zones"));
        generateBtn->setEnabled(false);
    }
    ctlLabel->setText(QString::number(athleteCTL, 'f', 1));
    atlLabel->setText(QString::number(athleteATL, 'f', 1));
    tsbLabel->setText(QString::number(athleteTSB, 'f', 1));
}

void WorkoutGeneratorDialog::addPoint(double minutes, double watts)
{
    generatedWorkout->Points.append(ErgFilePoint(minutes * 60000.0, watts, watts));
}

void WorkoutGeneratorDialog::addLap(double minutes, const QString &name)
{
    int lapNum = generatedWorkout->Laps.size() + 1;
    generatedWorkout->Laps.append(ErgFileLap(minutes * 60000.0, lapNum, name));
}

void WorkoutGeneratorDialog::addText(double minutes, int durationSecs, const QString &text)
{
    generatedWorkout->Texts.append(ErgFileText(minutes * 60000.0, durationSecs, text));
}

void WorkoutGeneratorDialog::addWarmup()
{
    double startWatts = athleteFTP * 0.50;
    double endWatts = athleteFTP * 0.75;

    addPoint(0, startWatts);
    addPoint(WARMUP_MINUTES, endWatts);
    addText(0, 30, tr("Warm up - easy spinning, gradually increase"));
}

void WorkoutGeneratorDialog::addCooldown(double startTime)
{
    double startWatts = athleteFTP * 0.55;
    double endWatts = athleteFTP * 0.40;

    addLap(startTime, tr("Cool Down"));
    addPoint(startTime, startWatts);
    addPoint(startTime + COOLDOWN_MINUTES, endWatts);
    addText(startTime, 20, tr("Cool down - easy spinning"));
}

int WorkoutGeneratorDialog::scaledIntervalCount(int baseCount, double cycleMinutes, double availableMinutes)
{
    // Scale by fitness level
    double ctlFactor = (athleteCTL > 0) ? std::max(0.6, std::min(1.5, athleteCTL / 70.0)) : 0.8;
    int scaled = qRound(baseCount * ctlFactor);
    scaled = std::max(2, scaled);

    // Cap by available time
    int maxFit = static_cast<int>(availableMinutes / cycleMinutes);
    return std::min(scaled, std::max(1, maxFit));
}

void WorkoutGeneratorDialog::generateRecovery(double available)
{
    double watts = athleteFTP * 0.50;
    double time = WARMUP_MINUTES;

    addLap(time, tr("Recovery"));
    addText(time, 30, tr("Recovery - keep it very easy"));

    // Gentle sine wave between 45-55% FTP
    double steps = available / 2.0;
    int numSteps = std::max(1, static_cast<int>(steps));
    for (int i = 0; i <= numSteps; i++) {
        double t = time + (available * i / numSteps);
        double w = athleteFTP * (0.50 + 0.05 * sin(2.0 * M_PI * i / numSteps));
        addPoint(t, w);
    }
    Q_UNUSED(watts);
}

void WorkoutGeneratorDialog::generateEndurance(double available)
{
    double time = WARMUP_MINUTES;

    addLap(time, tr("Endurance"));
    addText(time, 30, tr("Endurance - steady effort"));

    // Gentle undulation between 65-75% FTP
    double steps = available / 3.0;
    int numSteps = std::max(2, static_cast<int>(steps));
    for (int i = 0; i <= numSteps; i++) {
        double t = time + (available * i / numSteps);
        double w = athleteFTP * (0.70 + 0.05 * sin(2.0 * M_PI * i / numSteps));
        addPoint(t, w);
    }
}

void WorkoutGeneratorDialog::generateSweetSpot(double available)
{
    double workMins = 8.0, restMins = 4.0;
    double cycle = workMins + restMins;
    int intervals = scaledIntervalCount(4, cycle, available);

    double time = WARMUP_MINUTES;
    double workWatts = athleteFTP * 0.90;
    double restWatts = athleteFTP * 0.55;

    for (int i = 0; i < intervals; i++) {
        addLap(time, tr("Sweet Spot %1").arg(i + 1));
        addText(time, 15, tr("Interval %1 - sweet spot effort").arg(i + 1));
        addPoint(time, workWatts);
        addPoint(time + workMins, workWatts);

        time += workMins;
        addText(time, 10, tr("Recovery - spin easy"));
        addPoint(time, restWatts);
        addPoint(time + restMins, restWatts);
        time += restMins;
    }
}

void WorkoutGeneratorDialog::generateThreshold(double available)
{
    double workMins = 5.0, restMins = 5.0;
    double cycle = workMins + restMins;
    int intervals = scaledIntervalCount(4, cycle, available);

    double time = WARMUP_MINUTES;
    double workWatts = athleteFTP * 1.00;
    double restWatts = athleteFTP * 0.55;

    for (int i = 0; i < intervals; i++) {
        addLap(time, tr("Threshold %1").arg(i + 1));
        addText(time, 15, tr("Interval %1 - at threshold").arg(i + 1));
        addPoint(time, workWatts);
        addPoint(time + workMins, workWatts);

        time += workMins;
        addText(time, 10, tr("Recovery - spin easy"));
        addPoint(time, restWatts);
        addPoint(time + restMins, restWatts);
        time += restMins;
    }
}

void WorkoutGeneratorDialog::generateVO2max(double available)
{
    double workMins = 3.0, restMins = 3.0;
    double cycle = workMins + restMins;
    int intervals = scaledIntervalCount(5, cycle, available);

    double time = WARMUP_MINUTES;
    double workWatts = athleteFTP * 1.15;
    double restWatts = athleteFTP * 0.50;

    for (int i = 0; i < intervals; i++) {
        addLap(time, tr("VO2max %1").arg(i + 1));
        addText(time, 15, tr("Interval %1 - VO2max effort!").arg(i + 1));
        addPoint(time, workWatts);
        addPoint(time + workMins, workWatts);

        time += workMins;
        addText(time, 10, tr("Recovery - spin easy"));
        addPoint(time, restWatts);
        addPoint(time + restMins, restWatts);
        time += restMins;
    }
}

void WorkoutGeneratorDialog::generateAnaerobic(double available)
{
    double workMins = 0.5, restMins = 4.5;
    double cycle = workMins + restMins;
    int intervals = scaledIntervalCount(8, cycle, available);

    double time = WARMUP_MINUTES;
    double workWatts = athleteFTP * 1.50;
    double restWatts = athleteFTP * 0.40;

    for (int i = 0; i < intervals; i++) {
        addLap(time, tr("Anaerobic %1").arg(i + 1));
        addText(time, 10, tr("Sprint %1 - max effort!").arg(i + 1));
        addPoint(time, workWatts);
        addPoint(time + workMins, workWatts);

        time += workMins;
        addText(time, 10, tr("Recovery - easy spinning"));
        addPoint(time, restWatts);
        addPoint(time + restMins, restWatts);
        time += restMins;
    }
}

void WorkoutGeneratorDialog::generateMixed(double available)
{
    double time = WARMUP_MINUTES;

    // 10 min endurance block
    double enduranceMins = std::min(10.0, available * 0.25);
    addLap(time, tr("Endurance"));
    addText(time, 20, tr("Endurance block - steady"));
    addPoint(time, athleteFTP * 0.70);
    addPoint(time + enduranceMins, athleteFTP * 0.70);
    time += enduranceMins;

    double remaining = available - enduranceMins;

    // Threshold intervals (first 60% of remaining)
    double thresholdTime = remaining * 0.6;
    double thWorkMins = 4.0, thRestMins = 3.0;
    double thCycle = thWorkMins + thRestMins;
    int thIntervals = scaledIntervalCount(3, thCycle, thresholdTime);

    for (int i = 0; i < thIntervals; i++) {
        addLap(time, tr("Threshold %1").arg(i + 1));
        addText(time, 15, tr("Threshold interval %1").arg(i + 1));
        addPoint(time, athleteFTP * 1.00);
        addPoint(time + thWorkMins, athleteFTP * 1.00);
        time += thWorkMins;

        addPoint(time, athleteFTP * 0.55);
        addPoint(time + thRestMins, athleteFTP * 0.55);
        time += thRestMins;
    }

    // VO2max intervals (remaining 40%)
    double vo2Time = remaining * 0.4;
    double voWorkMins = 2.0, voRestMins = 2.0;
    double voCycle = voWorkMins + voRestMins;
    int voIntervals = scaledIntervalCount(3, voCycle, vo2Time);

    for (int i = 0; i < voIntervals; i++) {
        addLap(time, tr("VO2max %1").arg(i + 1));
        addText(time, 15, tr("VO2max interval %1 - hard!").arg(i + 1));
        addPoint(time, athleteFTP * 1.15);
        addPoint(time + voWorkMins, athleteFTP * 1.15);
        time += voWorkMins;

        addPoint(time, athleteFTP * 0.50);
        addPoint(time + voRestMins, athleteFTP * 0.50);
        time += voRestMins;
    }
}

void WorkoutGeneratorDialog::onGenerate()
{
    if (athleteFTP <= 0) return;

    delete generatedWorkout;
    generatedWorkout = new ErgFile(context);

    double totalMinutes = durationSpin->value();
    double available = totalMinutes - WARMUP_MINUTES - COOLDOWN_MINUTES;
    if (available < 5) available = 5;

    addWarmup();

    int type = workoutTypeCombo->currentData().toInt();
    switch (type) {
    case 0: generateRecovery(available); break;
    case 1: generateEndurance(available); break;
    case 2: generateSweetSpot(available); break;
    case 3: generateThreshold(available); break;
    case 4: generateVO2max(available); break;
    case 5: generateAnaerobic(available); break;
    case 6: generateMixed(available); break;
    }

    addCooldown(WARMUP_MINUTES + available);

    // Set metadata
    QString typeName = workoutTypeCombo->currentText();
    generatedWorkout->format(ErgFileFormat::erg);
    generatedWorkout->version("2");
    generatedWorkout->units("ENGLISH");
    generatedWorkout->ftp(athleteFTP);
    generatedWorkout->name(QString("AI %1 %2min").arg(typeName).arg(durationSpin->value()));
    generatedWorkout->description(QString("Generated %1 workout, %2 minutes, FTP=%3, CTL=%4")
        .arg(typeName)
        .arg(durationSpin->value())
        .arg(athleteFTP)
        .arg(athleteCTL, 0, 'f', 0));
    generatedWorkout->source("GoldenCheetah Workout Generator");
    generatedWorkout->duration(static_cast<long>(totalMinutes * 60000.0));

    generatedWorkout->finalize();

    updatePreview();
    saveBtn->setEnabled(true);
}

void WorkoutGeneratorDialog::updatePreview()
{
    if (!generatedWorkout || generatedWorkout->Points.isEmpty()) return;

    QVector<double> xData, yData;
    for (const ErgFilePoint &p : generatedWorkout->Points) {
        xData.append(p.x / 60000.0);
        yData.append(p.y);
    }

    powerCurve->setSamples(xData, yData);

    double maxY = *std::max_element(yData.begin(), yData.end());
    previewPlot->setAxisScale(QwtAxis::XBottom, 0, xData.last());
    previewPlot->setAxisScale(QwtAxis::YLeft, 0, maxY * 1.15);
    previewPlot->replot();
}

QString WorkoutGeneratorDialog::generateFilename()
{
    QString type = workoutTypeCombo->currentText().replace(" ", "");
    int duration = durationSpin->value();
    QString date = QDate::currentDate().toString("yyyyMMdd");
    return QString("AI_%1_%2min_%3").arg(type).arg(duration).arg(date);
}

void WorkoutGeneratorDialog::onSave()
{
    if (!generatedWorkout) return;

    QString workoutDir = appsettings->value(nullptr, GC_WORKOUTDIR, "").toString();
    if (workoutDir.isEmpty()) {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("No workout directory configured. Please set it in Options."));
        return;
    }

    QString baseName = generateFilename();
    QString filepath = workoutDir + "/" + baseName + ".erg";

    // Ensure unique filename
    int suffix = 1;
    while (QFile::exists(filepath)) {
        filepath = workoutDir + "/" + baseName + "_" + QString::number(suffix++) + ".erg";
    }

    generatedWorkout->filename(filepath);

    QStringList errors;
    if (generatedWorkout->save(errors)) {
        trainDB->startLUW();
        trainDB->importWorkout(filepath, *generatedWorkout);
        trainDB->endLUW();

        QMessageBox::information(this, tr("Workout Saved"),
            tr("Workout saved to:\n%1").arg(filepath));
        saveBtn->setEnabled(false);
    } else {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("Failed to save workout:\n%1").arg(errors.join("\n")));
    }
}
