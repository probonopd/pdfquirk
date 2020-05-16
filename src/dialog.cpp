/*
  This file is part of pdfquirk.
  Copyright 2020, Klaas Freitag <kraft@freisturz.de>

  pdfquirk is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  pdfquirk is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with pdfquirk.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dialog.h"
#include "./ui_dialog.h"

#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QDialogButtonBox>
#include <QSettings>
#include <QDir>
#include <QObject>
#include <QResizeEvent>
#include <QDebug>
#include <QtGlobal>

#include "imagelistdelegate.h"
#include "pdfcreator.h"



bool SizeCatcher::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        QResizeEvent *sizeEvent = static_cast<QResizeEvent *>(event);
        int h = sizeEvent->size().height();
        qDebug() << "caught height" << h;
        const QSize s(qRound(h/1.41), h);
        emit thumbSize(s);
    }
    return QObject::eventFilter(obj, event);
}

// ==================================================================

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    // Prepare a settings file
    const QString configHome { QString("%1/.config/pdfquirkrc").arg(QDir::homePath()) };
    _settings.reset(new QSettings(configHome, QSettings::IniFormat));

    // Thumbnail sizes
    int thumbWidth = 100;
    int thumbHeight = 141;

    ui->setupUi(this);

    ui->listviewThumbs->setModel(&_model);
    ImageListDelegate *delegate = new ImageListDelegate();
    ui->listviewThumbs->setItemDelegate(delegate);
    ui->listviewThumbs->setSelectionMode(QAbstractItemView::NoSelection);

    QFont f = ui->listviewThumbs->font();
    QFontMetrics fm(f);
    thumbWidth += 4;
    thumbHeight += 4 + fm.height() + 2;
    ui->listviewThumbs->setFixedHeight(thumbHeight+6);

    delegate->setSizeHint(thumbWidth, thumbHeight);

    // size catcher
    //  SizeCatcher *sizeCatcher = new SizeCatcher;
    // connect(sizeCatcher, &SizeCatcher::thumbSize, delegate, &ImageListDelegate::slotThumbSize);
    // ui->listviewThumbs->installEventFilter(sizeCatcher);

    connect (ui->pbAddFromFile, &QPushButton::clicked, this, &Dialog::slotFromFile);
    connect (ui->pbAddFromScanner, &QPushButton::clicked, this, &Dialog::slotFromScanner);

    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &Dialog::slotButtonClicked);

    updateInfoText();
}

void Dialog::slotButtonClicked(QAbstractButton *button)
{
    if (!button) return;
    const QString path = _settings->value(_SettingsLastFilePath, QDir::homePath()).toString();

    if( button == ui->buttonBox->button(QDialogButtonBox::StandardButton::Save)) {
        QStringList files = _model.files();

        const QString saveFile = QFileDialog::getSaveFileName(this, tr("Save PDF File"), path, "PDF (*.pdf)");
        if (!saveFile.isEmpty()) {
            PdfCreator *creator = new PdfCreator;
            connect(creator, &PdfCreator::finished, this, &Dialog::pdfCreatorFinished);
            creator->setOutputFile(saveFile);
            QApplication::setOverrideCursor(Qt::WaitCursor);
            creator->buildPdf(files);
        }
    }
}

void Dialog::accept()
{
    // do nothing to not close the dialog.
}

void Dialog::pdfCreatorFinished(bool success)
{
    QApplication::restoreOverrideCursor();

    // get the result file name from the creator object.
    QString resultFile;
    PdfCreator *creator = static_cast<PdfCreator*>(sender());
    if (creator) {
        resultFile = creator->outputFile();
        creator->deleteLater();
    }
    if (success) {
        _model.clear();
    }
    updateInfoText(resultFile);
}

void Dialog::slotFromFile()
{
    QString path = _settings->value(_SettingsLastFilePath, QDir::homePath()).toString();

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Add files to PDF"), path, "Images (*.png *.jpeg *.jpg)");
    for (const QString& file : files) {
        QFileInfo fi(file);
        qApp->processEvents();
        _model.addImageFile(file);

        path = fi.path();
    }
    updateInfoText();
    _settings->setValue(_SettingsLastFilePath, path);
    _settings->sync();
}

void Dialog::updateInfoText(const QString& saveFile)
{
    QString text = tr("No images loaded. Load from scanner or file using the buttons above.");
    if (_model.rowCount() > 0) {
        text = tr("%1 image(s) loaded. Press Save button to create PDF.").arg(_model.rowCount());
    }
    bool openExt { false };

    if (!saveFile.isEmpty()) {
        text = tr("PDF file was saved to <a href=\"file:%1\">%1</a>").arg(saveFile);
        openExt = true;
    }
    ui->labInfo->setOpenExternalLinks(openExt);
    ui->labInfo->setText(text);
    ui->buttonBox->button(QDialogButtonBox::StandardButton::Save)->setEnabled( _model.rowCount() > 0 );
}


void Dialog::slotFromScanner()
{

}

Dialog::~Dialog()
{
    delete ui;
}
