# ATM-simulator

- am folosit o structura numita "User" pentru a retine datele citite din users_data_file, si daca un client este logat;
- am plecat de la scheletul de la laboratorul 8;

- pentru fiecare comanda inafara de login, inainte de a trimite-o, am adaugat la finalul ei numarul de card
	ce este un identificator unic al clientului; -> pentru a putea stii cine a trimis mesajul;
	
	- ex: mesajul: "listsold" va fi trimis catre server ca "listsold <numar_card>";
- iar serverul va interpreta mesajul "listsold", asa cum este dorit;
