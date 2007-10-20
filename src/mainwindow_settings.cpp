/***************************************************************************
 *   Copyright (C) 2006 by EB   *
 *      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "mainwindow_settings.h"
#include "mainwindow.h"


/*
 *   Host - Class to save Host info
 */

Host::Host(const QString &title, const QString &host, const QString &address ,const unsigned int port, const QString &login, const QString &pass, const QString &cod)
{
	t = title;
	h = host;
	ad = address;
	pt = port;
	lg = login;
	pw = pass;
	cdc = cod;
}

/*
 *   List to help keeping things sorted
 */

int HostList::compareItems(Host* d1, Host* d2)
{
	Host *s1 = static_cast<Host*>(d1);
	Host *s2 = static_cast<Host*>(d2);

	Q_CHECK_PTR(s1);
	Q_CHECK_PTR(s2);

	if (s1 > s2)
		return 1;
	else if (s1 < s2)
		return -1;
	else
		// s1 == s2;
		return 0;
}


/*
 *   account & caption
 */

Account::Account(QWidget* parent)
{
	// init
	this->parent = parent;
	standard = PACKAGE + QString("V") + VERSION;

	set_offline();
}

Account::~Account()
{
}

// set caption
void Account::set_caption()
{
	if ((gsName == GS_UNKNOWN) ||  acc_name.isEmpty() ||  acc_name.isNull())
	{
		// server unknown or no account name
		// -> standard caption
		parent->setWindowTitle(standard);
	}
	else
	{
		if (status == GUEST)
			parent->setWindowTitle(svname + " - " + acc_name + " (guest)");
		else
			parent->setWindowTitle(svname + " - " + acc_name);
	}
}

// set go server name
void Account::set_gsname(GSName gs)
{
	gsName = gs;

	// now we know which server
	switch (gsName)
	{
		case IGS: svname.sprintf("IGS");
			break;

		case NNGS: svname.sprintf("NNGS");
			break;

		case LGS: svname.sprintf("LGS");
			break;

		case WING: svname.sprintf("WING");
			break;

		case CTN: svname.sprintf("CTN");
			break;

		case CWS: svname.sprintf("CWS");
			break;

		default: svname.sprintf("unknown Server");
			break;
	}

	// set account name
	if ( acc_name.isEmpty() ||  acc_name.isNull())
	{
		// acc_name should be set...
		acc_name.sprintf("Lulu");
		qWarning("set_gsname() - acc_name not found!");
	}
	
	if (status == OFFLINE)
		status = (enum Status) REGISTERED;
}

// set account name
void Account::set_accname(QString &name)
{
	acc_name = name;
}

// set status
void Account::set_status(Status s)
{
	status = s;
}

// set to offline mode
void Account::set_offline()
{
	gsName = GS_UNKNOWN;
	svname = (QString) NULL;
	acc_name = (QString) NULL;
	status = OFFLINE;

	set_caption();

	num_players = 0;
	num_watchedplayers = 0;
	num_observedgames = 0;
	num_games = 0;
}

// get some variables
Status Account::get_status()
{
	return status;
}

GSName Account::get_gsname()
{
	return gsName;
}

void PlayerTableItem::set_nmatchSettings(Player *p)
{
	nmatch = p->nmatch;

	nmatch_black = 		p->nmatch_black ;
	nmatch_white = 		p->nmatch_white;
	nmatch_nigiri = 	p->nmatch_nigiri ;
	nmatch_handicapMin = 	p->nmatch_handicapMin;
	nmatch_handicapMax  = 	p->nmatch_handicapMax;
	nmatch_timeMin = 	p->nmatch_timeMin;
	nmatch_timeMax  = 	p->nmatch_timeMax;
	nmatch_BYMin = 		p->nmatch_BYMin;
	nmatch_BYMax = 		p->nmatch_BYMax;
	nmatch_stonesMin = 	p->nmatch_stonesMin;
	nmatch_stonesMax = 	p->nmatch_stonesMax;
	nmatch_KoryoMin = 	p->nmatch_KoryoMin;
	nmatch_KoryoMax = 	p->nmatch_KoryoMax ;

	nmatch_settings =  !(p->nmatch_settings == "No match conditions");

}


/*
 * a cancel button has been pressed on the preference pages  
 */
void MainWindow::slot_cancelPressed()
{
	loadSettings();
}

/*
 * a page has been left. If it's a preference or server stting page, we check the settings
 */
void MainWindow::slot_currentChanged(int i)
{
	static int former=-1;
	QSettings settings;
	bool resend = FALSE;

	if ((former == 3) || (former == 4))
	{
		//Checks wether the nmatch parameters have been modified, in order to send a new nmatchrange command
		resend=((settings.value("NMATCH_BLACK").toBool() != ui.checkBox_Nmatch_Black->isChecked()) || 
			(settings.value("NMATCH_WHITE").toBool() != ui.checkBox_Nmatch_White->isChecked()) ||
			(settings.value("NMATCH_NIGIRI").toBool() != ui.checkBox_Nmatch_Nigiri->isChecked()) ||
			(settings.value("NMATCH_MAIN_TIME").toInt() != ui.timeSpin_Nmatch->value()) ||
			(settings.value("NMATCH_BYO_TIME").toInt() != ui.BYSpin_Nmatch->value()) ||
			(settings.value("NMATCH_HANDICAP").toInt() != ui.HandicapSpin_Nmatch->value()) ||
			(settings.value("DEFAULT_SIZE").toInt() != ui.boardSizeSpin->value()) ||
			(settings.value("DEFAULT_TIME").toInt() != ui.timeSpin->value()) ||
			(settings.value("DEFAULT_BY").toInt() != ui.BYSpin->value()) );

		saveSettings();
		if (resend)
			sendNmatchParameters();
	}

	former = i;
	
}




/*
 * saves the parameters on the 2 lats tabs into the QSettings 
 */
void MainWindow::saveSettings()
{
	QSettings settings;

	settings.setValue("LANGUAGE",ui.comboBox_language->currentIndex ());
//	settings.setValue("COMPUTER_PATH", ui.LineEdit_computer->text());

	settings.setValue("SKIN", ui.LineEdit_goban->text()); 
	settings.setValue("SKIN_TABLE", ui.LineEdit_Table->text()); 

	settings.setValue("TIMER_INTERVAL", ui.timerComboBox->currentIndex());

	int i = 0;
	if ( ui.radioButtonStones_2D->isChecked())
		i=1;
	else if ( ui.radioButtonStones_3D->isChecked())
		i=2;
	settings.setValue("STONES_LOOK", i);

	i = 0;
	if ( ui.radioButton_noSound->isChecked())
		i=1;
	else if ( ui.radioButton_myGamesSound->isChecked())
		i=2;
	settings.setValue("SOUND", i);


	//saves hosts list
	settings.beginWriteArray("HOSTS");
	for (int i = 0; i < hostlist.size(); ++i) 
	{
		settings.setArrayIndex(i);
		settings.setValue("title", hostlist.at(i)->title());
		settings.setValue("server", hostlist.at(i)->host());
		settings.setValue("address", hostlist.at(i)->address());
		settings.setValue("port", hostlist.at(i)->port());
		settings.setValue("loginName", hostlist.at(i)->loginName());
		settings.setValue("password", hostlist.at(i)->password());
		settings.setValue("codec", hostlist.at(i)->codec());
	}
	settings.endArray();

	settings.setValue("ACCOUNT",ui.cb_connect->currentIndex ());

	//server games default values
	settings.setValue("DEFAULT_KOMI",ui.komiSpinDefault->value() );
	settings.setValue("DEFAULT_SIZE",ui.boardSizeSpin->value() );
	settings.setValue("DEFAULT_TIME",ui.timeSpin->value() );
	settings.setValue("DEFAULT_BY",ui.BYSpin->value() );

	settings.setValue("NMATCH_BLACK", ui.checkBox_Nmatch_Black->isChecked());
	settings.setValue("NMATCH_WHITE", ui.checkBox_Nmatch_White->isChecked());
	settings.setValue("NMATCH_NIGIRI",ui.checkBox_Nmatch_Nigiri->isChecked());
	settings.setValue("NMATCH_MAIN_TIME", ui.timeSpin_Nmatch->value());
	settings.setValue("NMATCH_BYO_TIME", ui.BYSpin_Nmatch->value());
	settings.setValue("NMATCH_HANDICAP", ui.HandicapSpin_Nmatch->value());



	settings.setValue("AUTOSAVE", ui.CheckBox_autoSave->isChecked());
	settings.setValue("AUTOSAVE_PLAYED", ui.CheckBox_autoSave_Played->isChecked());


	//server byo yomi warning
	settings.setValue("BYO_SOUND_WARNING", ui.ByoSoundWarning->isChecked());
	settings.setValue("BYO_SEC_WARNING",ui.ByoSecWarning->value());

	//SGF edition tab default values
	settings.setValue("EDIT_SIZE",ui.newFile_Size->value());
	settings.setValue("EDIT_HANDICAP",ui.newFile_Handicap->value());
	settings.setValue("EDIT_KOMI",ui.newFile_Komi->text());

	


}



/*
 * loads the parameters from the QSettings into the 2 lats tabs
 */
void MainWindow::loadSettings()
{
	QSettings settings;

	ui.comboBox_language->setCurrentIndex (settings.value("LANGUAGE").toInt());
	ui.LineEdit_computer->setText(settings.value("COMPUTER_PATH").toString());

	ui.radioButtonStones_real->setChecked(TRUE);
	ui.radioButtonStones_2D->setChecked((settings.value("STONES_LOOK")==1));
	ui.radioButtonStones_3D->setChecked((settings.value("STONES_LOOK")==2));

	ui.radioButton_allGameSound->setChecked(TRUE);
	ui.radioButton_noSound->setChecked((settings.value("SOUND")==1));
	ui.radioButton_myGamesSound->setChecked((settings.value("SOUND")==2));

	ui.LineEdit_goban->setText(settings.value("SKIN").toString());
	ui.LineEdit_Table->setText(settings.value("SKIN_TABLE").toString());

	ui.timerComboBox->setCurrentIndex(settings.value("TIMER_INTERVAL").toInt());

	//server list
	hostlist.clear();
	ui.ListView_hosts->clear();
	ui.cb_connect->clear();
	Host *h;
	int size = settings.beginReadArray("HOSTS");
	for (int i = 0; i < size; ++i) 
	{
		settings.setArrayIndex(i);
		h = new Host( 	settings.value("title").toString(),
				settings.value("server").toString(),
				settings.value("address").toString(),
				settings.value("port").toInt(),
				settings.value("loginName").toString(),
				settings.value("password").toString(),
				settings.value("codec").toString());
		hostlist.append(h);

		QStringList sl;
		sl 	<< settings.value("title").toString()
			<< settings.value("server").toString()
			<< settings.value("port").toString()
			<< settings.value("loginName").toString()
			<< (h->password().isEmpty() ? "" : "***");

		new QTreeWidgetItem(ui.ListView_hosts, sl);		

		ui.cb_connect->addItem(h->title());

	}
 	settings.endArray();

	ui.cb_connect->setCurrentIndex(settings.value("ACCOUNT").toInt());


	//server games default values
	ui.komiSpinDefault->setValue(settings.value("DEFAULT_KOMI").toInt());
	ui.boardSizeSpin->setValue(settings.value("DEFAULT_SIZE").toInt());
	ui.timeSpin->setValue(settings.value("DEFAULT_TIME").toInt());
	ui.BYSpin->setValue(settings.value("DEFAULT_BY").toInt());

	ui.checkBox_Nmatch_Black->setChecked(settings.value("NMATCH_BLACK", QVariant(TRUE)).toBool());
	ui.checkBox_Nmatch_White->setChecked(settings.value("NMATCH_WHITE", QVariant(TRUE)).toBool());
	ui.checkBox_Nmatch_Nigiri->setChecked(settings.value("NMATCH_NIGIRI", QVariant(TRUE)).toBool());
	ui.HandicapSpin_Nmatch->setValue(settings.value("NMATCH_HANDICAP", QVariant(8)).toInt());	
	ui.timeSpin_Nmatch->setValue(settings.value("NMATCH_MAIN_TIME", QVariant(99)).toInt());
	ui.BYSpin_Nmatch->setValue(settings.value("NMATCH_BYO_TIME", QVariant(60)).toInt());

	ui.CheckBox_autoSave->setChecked(settings.value("AUTOSAVE").toBool());
	ui.CheckBox_autoSave_Played->setChecked(settings.value("AUTOSAVE_PLAYED").toBool());

	//server byo yomi warning
	ui.ByoSoundWarning->setChecked(settings.value("BYO_SOUND_WARNING").toBool());
	ui.ByoSecWarning->setValue(settings.value("BYO_SEC_WARNING").toInt());
	
	
	//SGF edition tab default values
	ui.newFile_Size->setValue(settings.value("EDIT_SIZE").toInt());
	ui.newFile_Handicap->setValue(settings.value("EDIT_HANDICAP").toInt());
	ui.newFile_Komi->setText(settings.value("EDIT_KOMI").toString());




}

/*
 * server tabs : button "add" clicked 
 */
void MainWindow::slot_addServer()
{
	// check if at least title and host inserted
	if (!ui.LineEdit_title->text().isEmpty() && !ui.comboBox_server->currentText().isEmpty())
	{
		// check if title already exists
		bool found = false;
		bool check;
		unsigned int tmp = ui.LineEdit_port->text().toUInt(&check);
		if (!check)
		{
			tmp = 0;
			qWarning("Failed to convert port to integer!");
		}

		Host *h;
		for (int i=0; i < hostlist.count() && !found ; i++)
		{
			h= hostlist.at(i);
			if (h->title() == ui.LineEdit_title->text())
			{
				found = true;
				// if found, insert at current pos, and remove old item
				delete hostlist.takeAt(i);
			}
		}
		
		// insert host (at it's sorted position) //TODO check wether this is needed
		hostlist.append(new Host(ui.LineEdit_title->text(),
				ui.comboBox_server->currentText(),
				ui.LineEdit_address->text(),
				tmp,
				ui.LineEdit_login->text(),
				ui.LineEdit_pass->text(),
				ui.ComboBox_codec->currentText()));


		QStringList sl;
		sl 	<< ui.LineEdit_title->text()
			<< ui.comboBox_server->currentText()
			<< QString::number(tmp)
			<< ui.LineEdit_login->text()
			<< (ui.LineEdit_pass->text().length() ? "***" : "");

		// create entry in listview
		if (!found)
		{
			new QTreeWidgetItem(ui.ListView_hosts, sl);
			ui.cb_connect->addItem(ui.LineEdit_title->text());
		}

		else
		{
			ui.ListView_hosts->currentItem()->setText(0, ui.LineEdit_title->text());
			ui.ListView_hosts->currentItem()->setText(1, ui.comboBox_server->currentText());
			ui.ListView_hosts->currentItem()->setText(2, QString::number(tmp));
			ui.ListView_hosts->currentItem()->setText(3, ui.LineEdit_login->text());
			ui.ListView_hosts->currentItem()->setText(4, (ui.LineEdit_pass->text().length() ? "***" : ""));
		}
//		ListView_hosts->repaint();
//			cb_title->insertItem(LineEdit_title->text(), 0);
/*
		// add to ClientWindow hostlist       !!! does not seem to be used !
		emit signal_addHost(LineEdit_title->text(),
		                    LineEdit_host->text(),
		                    tmp,
		                    LineEdit_login->text(),
		                    LineEdit_pass->text());
*/
	}

	// init insertion fields
	//slot_cbtitle(QString());
	ui.LineEdit_title->clear();
	
}


/*
 * server tabs : a line has been clicked on the list 
 */
void MainWindow::slot_clickedListView(QTreeWidgetItem *lvi ,  int )
{
	bool found = FALSE;
	Host *h;
	for (int i=0; i < hostlist.count() && !found ; i++)
	{
		h= hostlist.at(i);
		if (h->title() == lvi->text(0))
		{
			found = true;
		}
	}

	if (!found)
	{
		qDebug("Problem in hostlist : did not find entry title : %s" , lvi->text(0).toLatin1().constData());
		return;
	}

	ui.LineEdit_title->setText(h->title());
	ui.LineEdit_login->setText(h->loginName());
//	ui.LineEdit_login->setText(h->loginName());
	ui.LineEdit_port->setText(QString::number(h->port()));
	

}

/*
 * server tabs : 'new' button clicked : we clear the fields 
 */
void MainWindow::slot_new( )
{
	ui.LineEdit_title->clear();
	ui.LineEdit_login->clear();
//	ui.LineEdit_login->setText(h->loginName());
	ui.LineEdit_port->clear();
	ui.LineEdit_pass->clear();
	ui.LineEdit_address->clear();

}

/*
 * server tabs : an item in the server list has been selected 
 */
void MainWindow::slot_serverChanged( const QString &server)
{
	if (server == tr("Other"))
	{
		ui.LineEdit_port->clear();
		ui.LineEdit_port->setReadOnly(FALSE);
		ui.LineEdit_address->clear();
		ui.LineEdit_address->setReadOnly(FALSE);
	}
	else
	{
		ui.LineEdit_address->setReadOnly(TRUE);
		ui.LineEdit_port->setReadOnly(TRUE);

		if (server == QString("LGS"))
		{
			ui.LineEdit_title->clear();
			ui.LineEdit_address->setText("lgs.taiwango.net");
			ui.LineEdit_port->setText("9696");
//			ui.LineEdit_login->setText("guest");
			// What codec does this use?
			ui.ComboBox_codec->setCurrentIndex ( 0);//setCurrentText("");
		}
		else if (server == QString("WING"))
		{
			ui.LineEdit_title->clear();
			ui.LineEdit_address->setText("wing.gr.jp");
			ui.LineEdit_port->setText("1515");
//			ui.LineEdit_login->setText("guest");
			// What codec does this use?
			ui.ComboBox_codec->setCurrentIndex ( 0);
		}
		else if (server == QString("IGS"))
		{
			ui.LineEdit_title->clear();
			ui.LineEdit_address->setText("igs.joyjoy.net");
			ui.LineEdit_port->setText("7777");
//			ui.LineEdit_login->setText("guest");
			ui.ComboBox_codec->setCurrentIndex (1);
	
		}
	}
}

/*
 * The 'get engine' button has been pressed on the Go engine tab
 */
void MainWindow::slot_getComputerPath()
{
	QString fileName(QFileDialog::getOpenFileName(this, tr("Go engine"), "",
		tr("All Files (*)")));
	if (fileName.isEmpty())
		return;

  	ui.LineEdit_computer->setText(fileName);
}

/*
 * The engine path has been modified on the Go engine tab
 */
void MainWindow::slot_computerPathChanged(const QString & text)
{
	QSettings settings;

	settings.setValue("COMPUTER_PATH", text);
}



