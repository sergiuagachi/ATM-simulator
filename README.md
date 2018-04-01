# ATM-simulator

- am folosit o structură numită "User" pentru a reține datele citite din users_data_file, si dacă un client este logat;

- pentru fiecare comandă înafară de login, înainte de a trimite-o, am adăugat la finalul ei numarul de card
	ce este un identificator unic al clientului; -> pentru a putea ști cine a trimis mesajul;
	
	- ex: mesajul: "listsold" va fi trimis către server ca "listsold <numar_card>";
- iar serverul va interpreta mesajul "listsold", așa cum este dorit;
