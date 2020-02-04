#include "stdafx.h"
#include "Db.h"
#include "PatientHistory.h"

void Db::displayPatientNames() {
	for (int i = 0; i < patients.size(); i++) {
		cout << patients[i].getName() << '\n';
	}
}

PatientHistory Db::getPatientByName(string name) {
	PatientHistory ph;
	for (int i = 0; i < patients.size(); i++) {
		if (patients[i].getName() == name) {
			ph = patients[i];
			break;
		}
	}
	if (ph == NULL) {
		cout << "Patient not found" << '\n';
		return NULL;
	}

	return ph;
}

void Db::displayPatientHistory(string name) {
	PatientHistory ph = getPatientByName(name);
	if (ph == NULL) {
		return;
	}

	for (int i = 0; i < ph.getConsultations().size(); i++) {
		cout << ph.getConsultations()[i].first << ": " << ph.getConsultations()[i].second << '\n';
	}
}

void Db::updatePatientHistory(string name, string details) {
	PatientHistory ph = getPatientByName(name);
	if (ph == NULL) {
		return;
	}
	ph.getConsultations().push_back(make_pair(time(0), details));
}

void Db::populateDb() {
	PatientHistory *ph1 = new PatientHistory("Andrei");
	PatientHistory *ph2 = new PatientHistory("Marian");
	PatientHistory *ph3 = new PatientHistory("Andreea");
	patients.push_back(ph1);
	patients.insert(patients.end(), { ph2, ph3 });
	updatePatientHistory("Andrei", "Consult fara mentiuni extraordinare");
	updatePatientHistory("Andrei", "Faringoamigdalita");
	updatePatientHistory("Andreea", "Primul consult a decurs ok");
	updatePatientHistory("Andreea", "Fara boli cronice in familie");
	updatePatientHistory("Andreea", "Reclama dureri abdominale. Posibila apendicita");
	updatePatientHistory("Andreea", "Durerile persista. Trebuie sa mearga imediat la urgente.");
}

Db::Db()
{
}


Db::~Db()
{
}
