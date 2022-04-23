#pragma once

#include <QDialog>
#include "ui_about.h"


class About : public QDialog
{
	Q_OBJECT

public:
	About(QWidget* parent = Q_NULLPTR);
	~About();

private:
	Ui::About* ui;
private:
	void init_label();
private slots:
	void on_btn_Ok_clicked();
	void on_btn_Cancel_clicked();
};
