#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/types.h>
#include <unistd.h> 
#include <iostream>
#include <string>
#include <cstdio>
#include <fstream>
#include <errno.h>

using namespace std;

# define CHUNK 512000
# define IP_ADDR "127.0.0.1"
# define PORT_NO 15050

void send_status(int);
void printMsg();
void send_port();
void sock_connect(const char*, int);
void create_udp_socket();
void quit(int);
void send_data(string, int);
void send_file();
void recvData(int);
void recv_file(string,int);

//=====variabili socket ==========
int porta, ret, sd;
struct sockaddr_in srv_addr;
bool udp_sock_created = false;
fd_set master, read_fds;
int fdmax;
int udp_socket;
uint32_t udp_port;
struct sockaddr_in address;
//================================

string net_buf;

int stato, len;
long long int lmsg;
fstream fp; //puntatore al file da aprire
ofstream ofp;

int main()
{
	udp_port = PORT_NO;
	sock_connect(IP_ADDR, PORT_NO); //argv[0] è il comando ./client, argv[1]=porta client, argv[2] porta server
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	FD_SET(sd, &master);
	fdmax = sd;
	
	printMsg();

    	create_udp_socket();
while(1)
{	printf(">");
	fflush(stdout);
	read_fds = master;
	
	if(select(fdmax +1, &read_fds, NULL, NULL, NULL) == -1)
    	{
    		perror("SERVER: select() error.");
    		exit(1);
    	}

    	for(int i = 0; i<=fdmax; i++)
    	{	
    		if(FD_ISSET(i, &read_fds))
    		{	
    			if(i == 0)
    			{
    				cin>>net_buf;
    				
    				if(net_buf == "!help") {
					stato=0;
					printMsg(); }
				else if(net_buf == "!upload") 
					stato = 1;
				else if(net_buf ==  "!get")
					stato = 2;
				else if(net_buf == "!quit")
					stato = 3;
				else if(net_buf == "!list")
					stato = 4;
				else if(net_buf == "!squit")
					stato = 5;
				else stato = -1; //stato di errore
				
				switch(stato) 
				{
					case 0: //stato neutro
						break;
					case 1:
						cout<<"Inserire il nome del file da inviare: "<<endl;
						cin>>net_buf;
						
						fp.open(net_buf.c_str(), ifstream::binary); //apro il file in modalità binaria
				    	
				    		if(!fp) { cerr<<"ERRORE: apertura file non riuscita."<<endl; break; }
				    		else 
				    		{
				    			send_status(stato);
				    			cout<<"Apertura file eseguita correttamente."<<endl;
				    			
				    			
				    			send_data(net_buf, net_buf.length()); //invio il nome file
				    			
				    			send_file(); //invio il file vero e proprio			
				    		}
				    		
						break;
					case 2:
						send_status(stato);
						
						cout<<"Inserire il nome del file da scaricare: "<<endl;
						cin>>net_buf;
						
						send_data(net_buf, net_buf.length()); //invio nome al server e attendo conferma per il download
						
						//ricevo conferma esistenza file
						bool found;
						if(recv(sd, &found, sizeof(found), 0) == -1) //sostituito new_sd con i
						{
						    cerr<<"Errore in fase di recv() relativa all'esistenza del file. Codice: "<<errno<<endl;
						    exit(1);
						}
						
						if(!found) { cout<<"File inesistente!"<<endl; break; }
						else
						{
							cout<<"Il file è disponibile per il download."<<endl;
							cout<<"In attesa del file: "<<net_buf.c_str()<<endl;
							
							recv_file(net_buf.c_str(), sd);
						}
						
						break;
					case 3:
						send_status(stato);
						cout<<"Comando non ancora implementato..."<<endl;
						break;
					case 4:
						send_status(stato);
						recvData(sd);
						cout<<"======= FILE DISPONIBILI ========"<<endl;
						cout<<net_buf.c_str();
						cout<<"================================="<<endl;
						//cout<<"Comando non ancora implementato..."<<endl;
						break;
					case 5:
						send_status(stato);
						cout<<"Disconnessione in corso..."<<endl;
						quit(i);
						break;
					default:
						cout<<"Comando non riconosciuto. Riprovare."<<endl;
						break;
				} //chiusura switch
					
    			}
    		}
    	}
}

	return 0;
}

void recv_file(string filename, int new_sd)
{
	if(recv(new_sd, &lmsg, sizeof(uint64_t), MSG_WAITALL) == -1) {
		cerr<<"Errore in fase di ricezione lunghezza file. Codice: "<<errno<<endl; exit(1); }
      
	long long int fsize = ntohl(lmsg); // Rinconverto in formato host
	cout<<"Lunghezza file (Bytes): "<<fsize<<endl;							
	char *buf = new char[fsize];
//il flag WAITALL indica che la recv aspetta TUTTI i pacchetti. Senza ne riceve solo uno e quindi file oltre una certa dimensione risultano incompleti							
	
	cout<<"Ricezione di "<<filename<<" in corso..."<<endl;
	
	long long int mancanti = fsize;
	long long int ricevuti = 0;
	int count=0;
	char *app_buf = buf; //app punta a buf[0]
	int progress = 0;
	
	while((mancanti-CHUNK) > 0)
	{
		int n = recv(new_sd, (void*)buf, CHUNK, MSG_WAITALL);
		if(n == -1)
		{
			cerr<<"Errore in fase di ricezione buffer dati. Codice: "<<errno<<endl;
			exit(1);
		}

		ricevuti += n;
		mancanti -= n;
		
		buf += CHUNK; //mi sposto di CHUNK posizioni in avanti nell'array (vedi aritmetica dei puntatori)
		
		//percentuale di progresso
		progress = (ricevuti*100)/fsize;
		cout<<"\r"<<progress<<"%";

		count++;
	}
	if(mancanti != 0)
	{
		int n = recv(new_sd, (void*)buf, (fsize-ricevuti), MSG_WAITALL);
		if(n == -1)
		{
			cerr<<"Errore in fase di ricezione buffer dati. Codice: "<<errno<<endl;
			exit(1);
		}
		ricevuti += n;
		progress = (ricevuti*100)/fsize;
		cout<<"\r"<<progress<<"%";

		count++;
	}
	cout<<endl;
	cout<<"Ricevuto file in "<<count<<" pacchetti, per un totale di "<<ricevuti<<" bytes."<<endl;
	
	cout<<"Salvataggio file in corso. Attendere..."<<endl;

	ofp.open(filename, ofstream::binary); //creo il file con il nome passato
	
	if(!ofp) { cerr<<"Errore apertura file."<<endl; }
	ofp.write(app_buf, fsize); //scrivo tutto app_buf (lungo fsize) nel file
								
	cout<<"Salvataggio file completato."<<endl;
	ofp.close();
	cout<<"File chiuso."<<endl;
}

void send_file()
{
	fp.seekg(0, fp.end); //scorro alla fine del file per calcolare la lunghezza (in Byte)
	long long int fsize = fp.tellg(); //fsize conta il num di "caratteri" e quindi il numero di byte --> occhio che se dim file > del tipo int ci sono problemi
	fp.seekg(0, fp.beg); //mi riposizione all'inizio
	
	cout<<"Lunghezza file(Byte): "<<fsize<<endl;
	char *buf = new char[fsize]; //buffer di appoggio per l'invio su socket
	fp.read(buf, fsize); //ora buf contiene il contenuto del file letto
	
	fp.close();
	
	lmsg = htonl(fsize); //invio lunghezza file
	cout<<"lmsg = htonl(fsize): "<<lmsg<<endl;//", sizeof(uint32_t): "<<sizeof(uint32_t)<<endl;
	if(send(sd, &lmsg, sizeof(uint64_t), 0) == -1)
	{
		cerr<<"Errore di send(size)."<<endl;
		exit(1);
	}
	
	cout<<"Invio del file: "<<net_buf<<" in corso..."<<endl;
	
	long long int mancanti = fsize;
	long long int inviati = 0;
	int count=0, progress=0;
	//=========il problema potrebbe essere di ricopiatura dell'array=========
	while((mancanti-CHUNK)>0)
	{
		int n = send(sd, (void*)buf, CHUNK, 0);
		//cout<<"send(): "<<n<<endl;
		if(n == -1)
		{
			cerr<<"Errore di send(buf)."<<endl;;
			exit(1);
		}
		count++;
		
		buf += CHUNK;
		mancanti -= n;
		inviati += n;
		
		progress = (inviati*100)/fsize;
		cout<<"\r"<<progress<<"%";	
	}
	if(mancanti!=0)
	{
		int n = send(sd, (void*)buf, mancanti, 0);
		//cout<<"send(): "<<n<<endl;
		if(n == -1)
		{
			cerr<<"Errore di send(buf)."<<endl;;
			exit(1);
		}
		count++;
		inviati += n;
		progress = (inviati*100)/fsize;
		cout<<"\r"<<progress<<"%";	
	}
	cout<<endl;
	cout<<"Inviato file in "<<count<<" pacchetti."<<endl;
	//cout<<"File inviato."<<endl;
}

void recvData(int sd)
{
	// Attendo dimensione del mesaggio                
	if(recv(sd, (void*)&lmsg, sizeof(uint32_t), 0) == -1)
	{
		cerr<<"Errore in fase di ricezione lunghezza. Codice: "<<errno<<endl;
		exit(1);
	}
	
	len = ntohl(lmsg); // Rinconverto in formato host

	if(recv(sd, (void*)net_buf.c_str(), len, MSG_WAITALL) == -1)
	{
		cerr<<"Errore in fase di ricezione buffer dati. Codice: "<<errno<<endl;
		exit(1);
	}
}

void send_data(string buf, int buf_len)
{		
	//len = strlen(buf)+48; //32 è la dim del MAC + 16 la dim di AES = 48
	//len = buf.length();
	lmsg = htons(buf_len);
	
	if(send(sd, (void*) &lmsg, sizeof(uint16_t), 0) == -1)
	{
		cerr<<"Errore di send(size)."<<endl;
		exit(1);
	}

        if(send(sd, (void*)buf.c_str(), buf_len, 0) == -1)
        {
        	cerr<<"Errore di send(buf)."<<endl;;
        	exit(1);
        }
        
        stato = -1;
}

void send_status(int stato)
{
	if(send(sd, (void*)&stato, sizeof(stato), 0)== -1)
	{
		cerr<<"Errore di send_status():"<<endl;
		exit(1);
	}
}

void printMsg()
{
	cout<<"Sono disponibili i seguenti comandi: "<<endl<<endl;
	cout<<"!help --> mostra l'elenco dei comandi disponibili "<<endl;
	cout<<"!upload --> carica un file presso il server "<<endl;
	cout<<"!get --> scarica un file dal server "<<endl;
	cout<<"!quit --> disconnette il client dal server ed esce "<<endl;
	cout<<"!list --> visualizza elenco file disponibili sul server "<<endl;
	cout<<"!squit --> SUPERQUIT: termina client e server "<<endl;
	cout<<endl;
}

void send_port()
{
	if(send(sd, (void*) &porta, sizeof(porta), 0) == -1)
		cout<<"Errore di send(). "<<endl;
}

void create_udp_socket()
{
	int yes = 1;
	
	if((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		cerr<<"errore creazione socket UDP."<<endl;
		exit(1);
	}
	
	address.sin_family = AF_INET;
    	address.sin_port = htons(udp_port);
    	address.sin_addr.s_addr = htonl(INADDR_ANY);
    	
    	if(setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    	{
    		cerr<<"Errore di setsockopt."<<endl;
    		exit(1);
    	}
    	
    	if(bind(udp_socket, (struct sockaddr*)&address, sizeof(address)) == -1)
    	{
    		cerr<<"errore in fase di bind() udp."<<endl;
    		exit(1);
    	}
    	
    	if(udp_socket > fdmax)
    		fdmax = udp_socket;
    	FD_SET(udp_socket, &master);
	
	udp_sock_created = true;
}

void sock_connect(const char* address, int porta_server)
{
	/* Creazione socket */
    	sd = socket(AF_INET, SOCK_STREAM, 0);
    	
    	/* Creazione indirizzo del server */ 
    	srv_addr.sin_family = AF_INET;
    	srv_addr.sin_port = htons(porta_server);
    	inet_pton(AF_INET, address, &srv_addr.sin_addr);
    	
    	if(connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0)
   	{
        	cerr<<"Errore in fase di connessione: "<<endl;
        	exit(-1);
    	}
    	else
    	{
    		cout<<"Connessione al server "<<address<<" sulla porta "<<porta_server<<" effettuata con successo."<<endl;
    		cout<<"Ricezione messaggi istantanei su porta "<<porta<<"."<<endl<<endl;
    	}
}

void quit(int i)
{			
	FD_CLR(i, &master);
	close(sd);
	cout<<"Client disconnesso."<<endl;
	exit(1);
}