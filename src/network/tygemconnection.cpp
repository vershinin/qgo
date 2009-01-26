/* Actually this is eweiqi though I think they're all the same,
 * eventually we'll make tygem the super class */
#include <string.h>
#include "tygemconnection.h"
#include "tygemprotocolcodes.h"
#include "consoledispatch.h"
#include "roomdispatch.h"
#include "boarddispatch.h"
#include "gamedialogdispatch.h"
#include "talkdispatch.h"
#include "dispatchregistries.h"
#include "serverlistdialog.h"
#include "matchinvitedialog.h"
#include "gamedialogflags.h"
#include "orosetphrasechat.h"
#include "setphrasepalette.h"
#include "createroomdialog.h"
#include "gamedata.h"
#include "playergamelistings.h"
#include <QMessageBox>

#define RE_DEBUG

#define FIRST_BYTE(x)	(x >> 24) & 0x000000ff
#define SECOND_BYTE(x)	(x >> 16) & 0x000000ff
#define THIRD_BYTE(x)	(x >> 8) & 0x000000ff
#define FOURTH_BYTE(x)	x & 0x000000ff

#define ZODIAC_BYTE	0x9a
/* 57 is our zodiac sign 
		01 is rat, 02 is bull, 03 is tiger, 04 is rabbit
		05 is dragon, 06 is snake, 07 is horse
		08 is sheep (ram? goat?), 09 is monkey
		0a is rooster, 0b is dog, 0c is pig
		
		0d is mercury like chibi figure with golden shield
		0e is red warrior palace chibi
		0f is wild west gunman on horse
		10 is chibi in snow sweeping
		11 red warrior bamboo forest
		12 calligrapher
		13 red haired bureaucrat, purple cap?
		14 red haired bureacrat, scroll, white cap
		15 guy with shield on battlefield?
		16 two blade zhong warrior night time balcony
		17 same with no zhong, bigger blades
		24 man running with bunny
		30 sports uniform kid with korean flag
		34 child spinning soccer ball on finger
		35 small girl playing go on computer
		40 soccer kid
		43 woman reading book with headphones
		50 purple mouse, bowtie, cheese on platter
		55 snake around yellow stone
		56 horse
		57 giant chicken
		58 strange blue purple dog with bone around neck
		59 childs drawing of pig in love
		5a woman walking dog
		5b ballet dancer stretching
		5c woman on balcony looking out over ocean
		5d girl blue flowers in hair, blue dress
		5e 31 gangster on dark blue background
		5f pig tailed beach girl
		60 blackhaired girl rainbow very blue
		61 ghetto guy against graffiti wall
		62 green haired boxing girl on sun like background
		63 frightened cloaked guy holding skull
		64 fierce blonde pigtailed girl on blue sky
		65 dominatrix infront of night time castle
		66 pigtailed brunette princess in red and gold background
		67 studying girl
		68 strong guy saying something, korean letters, blue star hat
		69 woman purple sheath pink background
		6a black haired girl black dress, roses
		6b pink girl holding valentine at night with moon
		7e ninja nighttime, gold crown
		7f japanese warlord
		80 new tygem congratulations, white smiling figure, gold badge
		81 same with black smirking figure
		82 same with out gold badge
		90 green haired happy girl under night sky smiling moon?
		91 grey pig eating or happy at table with food
		92 2 kids in love
		93 bowl of orange fruit
		94 night life guy with flowers
		95 night life business lady?
		96 blonde smiling guy showbiz stars
		97 korean infinity girl picture
		98 white creature with brown hat, fall leaves
		99 white creatures riding dolphins
		9a white and black creatures playing game
		a0 thin santa claus, merry christmas
		b0 animated bouncing girl on pink hearts
		c0 korean letters, boiling chicken, and dancing chickens
		d0 scarecrow golden field
		e0 no picture
		FIXME */

TygemConnection::TygemConnection(class NetworkDispatch * _dispatch, const class ConnectionInfo & info)
{
	dispatch = _dispatch;
	
	username = QString(info.user);
	password = QString(info.pass);
	/*if(openConnection(info))
	{
		connectionState = LOGIN;
		
	}
	else
		qDebug("Can't open Connection\n");	//throw error?
*/
	textCodec = QTextCodec::codecForLocale();
	serverCodec = QTextCodec::codecForLocale();
	//FIXME check for validity of codecs ??? 
	//ORO_setup_setphrases();
	//serverCodec = QTextCodec::codecForName("GB2312");
	
	current_server_index = -1;
	encode_offset = 0;
	current_server_addr = 0;
	
	observing_game_count = 0;
	player_accepted_match = 0;
	
	room_were_in = 0;	//lobby?
	connecting_to_game_number = 0;
	playing_game_number = 0;
	our_game_being_played = 0;	//FIXME redundant
	
	matchKeepAliveTimerID = 0;
	matchRequestKeepAliveTimerID = 0;
	
	/* We should either create the palette on creation of
	 * the connection, or have a button to activate it that
	 * appears when the room is connected */
	//setphrasepalette = new SetPhrasePalette(this);
	//setphrasepalette->show();
	
	//for server list
	//error handling FIXME
	if(info.type == TypeTYGEM)
	{
		serverCodec = QTextCodec::codecForName("eucKR");
		requestServerInfo();
	}
}

int TygemConnection::requestServerInfo(void)
{
	qDebug("Requesting Tygem Server Info");
	ConnectionInfo server("121.189.9.52", 80, "", "", TypeNone);
	if(!openConnection(server))
	{
		qDebug("Can't get server info");
		return -1;
	}
	unsigned int length = 0x96;
	unsigned char * packet = new unsigned char[length];  //term 0x00
	snprintf((char *)packet, length,
		 "GET /service/down_korea/livebaduk3.cfg HTTP/1.1\r\n" \
		 "User-Agent: Tygem HTTP\r\n" \
		 "Host: service.tygem.com\r\n" \
		 "Connection: Keep-Alive\r\n" \
		 "Cache-Control: no-cache\r\n\r\n");
	for(int i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
	{
		qWarning("*** failed sending server info request");
		return -1;
	}
	delete[] packet;
	
	connectionState = INFO;
	return 0;
}

TygemConnection::~TygemConnection()
{
	//if(setphrasepalette)
	//	delete setphrasepalette;
	// FIXME for non tygem subclasses!
	qDebug("Destroying Tygem connection\n");
	closeConnection();
	std::vector<ServerItem*>::iterator iter;
	for(iter = serverList.begin(); iter != serverList.end(); iter++)
		delete *iter;
	if(current_server_addr)
		delete[] current_server_addr;
}

void TygemConnection::OnConnected()
{
}

void TygemConnection::sendText(QString text)
{
	return;
}

void TygemConnection::sendDisconnect(void)
{
	/* Yeah, FIXME, we don't want to send this
	 * when we're reconnecting... which means we
	 * need a special connection state ... yada yada */
	if(connectionState == CONNECTED)
		sendDisconnectMsg();
}

void TygemConnection::sendText(const char * text)
{
	qDebug("Wanted to send %s", text);
	return;
}

void TygemConnection::sendMsg(const PlayerListing & player, QString text)
{
	sendPersonalChat(player, text.toLatin1().constData());
}

void TygemConnection::sendToggle(const QString & param, bool val)
{
	/* FIXME, more to do here probably */
	qDebug("Setting open %d", val);
	if(param == "open")
		sendInvitationSettings(val);
}

/* If we can observe something besides games,
 * like rooms or people, then this name is a bit
 * iffy too... */
void TygemConnection::sendObserve(const GameListing & game)
{
	sendObserve(game.number);
}

/* Yeah, this is just a kind of sendJoin */
void TygemConnection::sendObserve(unsigned short game_number)
{
	unsigned int length = 24;
	char *packet = new char[length];
	int i;
	if(observing_game_count == 3)
	{
		//legit client won't let you watch more than 3 games
		qDebug("Maybe we can't watch more than 3 games");
		//message box, FIXME
		//it is allowed but probably taxes server, as server sets
		//time, etc... and who watches more than three games at a time...
		//besides people testing hacked together clients?
		return;
	}
	/* We need to turn this to a join if its a room, not a game
	* otherwise bad things happen FIXME */
	
	packet[0] = 0x00;
	packet[1] = 0x18;
	packet[2] = 0x06;
	packet[3] = 0x38;
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	packet[6] = 0x00;
	packet[7] = 0x00;	
	for(i = 8; i < 0x18; i++)
		packet[i] = 0x00;
#ifdef RE_DEBUG
	//printf("Sending observe  to %s vs %s: %d", game.white_name().toLatin1().constData(), game.black_name().toLatin1().constData(), game.game_code);
	printf("Sending observe to %d\n", game_number);
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode((unsigned char *)packet, (length / 4) - 2);
	printf("After encode\n");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending observe outside");
	delete[] packet;
	
	connecting_to_game_number = game_number;
}

/* This is sent after "sendObserve" when we join a match */
void TygemConnection::sendJoin(unsigned short game_number)
{
	unsigned int length = 12;
	char * packet = new char[length];
	int i;
	
	packet[0] = 0x00;
	packet[1] = 0x0c;
	packet[2] = 0x06;
	packet[3] = 0x65;
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	for(i = 6; i < 12; i++)
		packet[i] = 0x00;
#ifdef RE_DEBUG
	printf("Sending join %d", game_number);
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode((unsigned char *)packet, (length / 4) - 2);
	printf("After encode\n");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending observe outside");
	delete[] packet;
	
	connecting_to_game_number = game_number;
}

void TygemConnection::stopObserving(const GameListing & game)
{
	qDebug("stopObserving %d %d", room_were_in, game.number);
	/* These should probably be done by the closeBoard stuff.
	 * i.e., there isn't really a stopObserving */
#ifdef FIXME
	if(room_were_in == game.number)
		sendLeave(game);
	else
		sendFinishObserving(game);
#endif //FIXME
	/* Shouldn't igs, etc., have this also?!?!?! only adjourns
	 * seem to properly close through registry FIXME */
	//closeBoardDispatch(game.game_code);
}

void TygemConnection::stopReviewing(const GameListing & /*game_id*/)
{
	//FIXME
}

/* I'm thinking you can only play one game at a time on IGS,
 * but the game_ids are there in case it changes its mind */
void TygemConnection::adjournGame(const GameListing & game)
{
}

void TygemConnection::closeBoardDispatch(unsigned int game_id)
{
	sendFinishObserving((unsigned short)game_id);
	NetworkConnection::closeBoardDispatch(game_id);
}


/* I'd like to just send the match invite, but only if we're not
 * in the same room... actually, maybe its almost like a room invite */
void TygemConnection::sendMatchRequest(MatchRequest * mr)
{
	qDebug("send match offer");
	sendMatchOffer(*mr, offer);
}

/* FIXME gamedialog maintime supports m:ss, not h:mm:ss,
 * for some reason 90 minutes is getting set as 50 minutes
 * certainly not an hour and a half, but its sent okay.
 * but this is a BIG ISSUE for later, okay 5 hours screws
 * things up even more.*/
unsigned long TygemConnection::getGameDialogFlags(void)
{
	return (GDF_CANADIAN | GDF_BYOYOMI | /*GDF_TVASIA |*/ GDF_FREE_RATED_CHOICE 
		  | GDF_RATED_SIZE_FIXED | GDF_RATED_HANDICAP_FIXED
	          | GDF_NIGIRI_EVEN /*| GDF_ONLY_DISPUTE_TIME*/
		  | GDF_BY_CAN_MAIN_MIN);
}

void TygemConnection::declineMatchOffer(const PlayerListing & opponent)
{
	qDebug("Declining match offer");
	//FIXME
}

void TygemConnection::cancelMatchOffer(const PlayerListing & opponent)
{
	qDebug("Canceling match offer");
	//FIXME
}

void TygemConnection::acceptMatchOffer(const PlayerListing & /*opponent*/, MatchRequest * mr)
{
	qDebug("accept match offer");
	sendMatchOffer(*mr, accept);
}

/*void TygemConnection::setAccountAttrib(AccountAttib * aa)
{
		
}*/

void TygemConnection::handlePendingData(newline_pipe <unsigned char> * p)
{
	unsigned char * c;
	unsigned char header[4];
	unsigned int packet_size;
	int bytes;

	/* We need better connection states, but we'll use the old
	 * for now */
	
	switch(connectionState)
	{
		case INFO:
			bytes = p->canRead();
			if(bytes)
			{
				c = new unsigned char[bytes];
				p->read(c, bytes);
				/* First packet is list of servers and ips */
				handleServerInfo(c, bytes);
				delete[] c;
			}
			break;
		case LOGIN:
			bytes = p->canRead();
			if(bytes)
			{
				c = new unsigned char[bytes];
				p->read(c, bytes);
				for(int i = 0; i < bytes; i++)
					printf("%02x", c[i]);
				printf("\n");
				/* First packet is list of servers and ips */
				//handleServerList(c);
				
				//00080698ff040068		//password incorrect
				if(c[1] == 0x08)
				{
					//00080698ff010065	//maybe because too recent?
								//could also be already
								//logged in
					//00080698ff040013	//error on reconnect...
					if(c[7] == 0x68)
					{
						qDebug("Bad password or login");
						//FIXME dialogs??
						closeConnection();
						return;
					}
					else if(c[7] == 0x65)
					{
						//special response
						qDebug("Already logged in? Too recent?");
						sendLogin(true);
						/* FIXME its real ugly if this happens
						 * more than once and we hammer the
						 * server until it lets us in.  We need
						 * to sleep or put out a message or
						 * something. */
					}
					
				}
				else if(c[1] == 0x38)
					handleLogin(c, bytes);
				
				delete[] c;
			}
			break;
		case CONNECTED:
			while((bytes = p->canRead()))
			{
				p->peek(header, 4);
#ifdef RE_DEBUG
				printf("%02x%02x%02x%02x\n", header[0], header[1], header[2], header[3]);
#endif //RE_DEBUG
				packet_size = header[1] + (header[0] << 8);
				if((unsigned)bytes < packet_size)
					break;
				c = new unsigned char[packet_size];
				p->read(c, packet_size);
				handleMessage(c, packet_size);
				delete[] c;
			}
			break;
		case CANCELED:
		case RECONNECTING:
			break;
		/*case AUTH_FAILED:
			qDebug("Auth failed\n");
			break;*/
	}
}

/* Really we should have a legitimate parse function, but I feel
 * like this is the only time we're going to do this.
 * Yeah, FIXME, I've decided we should write a parser just because...*/
void TygemConnection::handleServerInfo(unsigned char * msg, unsigned int length)
{
	char * p = (char *)msg;
	int i, j;
	ServerItem * si;
	
	for(i = 0; i < length; i++)
		printf("%c", p[i]);
	printf("\n");
	
	if(strncmp(p, "HTTP/1.1 200 OK\r\n", 17) != 0)
	{
		qDebug("Server info response not OK!");
		closeConnection();
		return;
	}
	while(p < ((char *)msg + length))
	{
		while(p < ((char *)msg + length - 1) && (p[0] != '\r' || p[1] != '\n'))
			p++;
		p += 2;
		//FIXME, check for size
		if(strncmp(p, "/LIVE ", 6) == 0)
		{
			//same as for /SERVER but with only 2 [], name and ip
		}
		else if(strncmp(p, "/LECTURE ", 9) == 0)
		{
			//same as for /SERVER but with only 2 [], name and ip
		}
		else if(strncmp(p, "/SERVER ", 8) == 0 ||
		        strncmp(p, "/_SERVER ", 9) == 0)	//whats the "_" FIXME
		{
			if(p[1] == '_')
				p++;
			p += 8;
			unsigned char ipaddr[16];
			unsigned char name[20];
			i = 0;
			while(i < 7 && p < ((char *)msg + length))
			{
				if(*p != '[')
				{
					qDebug("Server info parse error");
					closeConnection();
					return;
				}
				p++;
				si = new ServerItem();
				unsigned char * dest;
				if(i == 1)
					dest = ipaddr;
				else if(i == 0)
					dest = name;
				j = 0;
				while(*p != ']' && p < ((char *)msg + length))
				{
					if((i == 1 && j < 15) || (i == 0 && j < 19))
						dest[j++] = *p;
					p++;
				}
				if(i == 0 || i == 1)
				{
					dest[j++] = 0x00;
				}
				p += 2;
				if(i == 1)
				{
					strncpy(si->ipaddress, (const char *)ipaddr, 16);
					si->name =  serverCodec->toUnicode((char *)name, strlen((char *)name));
					qDebug("Server: %s", ipaddr);
					serverList.push_back(si);
				}
				i++;
				if(i == 7)
					p--;	//no space
			}
		}
		else if(strncmp(p, "/ENCRYPT ", 9) == 0)
		{
			//FIXME
			//[encode] [on]	
		}
	}
	current_server_addr = new char[16];
	/* We close here because this first time, its going to close
	* anyway, and if we don't close here, we'll get an error
	* and lose the object */
	connectionState = RECONNECTING;
	closeConnection(false);
	
	if(reconnectToServer() < 0)
	{
		qDebug("User canceled");
		closeConnection(false);
		connectionState = CANCELED;
		if(dispatch)
			dispatch->onError();	//not great... FIXME
		return;
	}
	//sendLogin();
}

/* Response bit is also server change/reconnect bit */
void TygemConnection::sendLogin(bool response_bit)
{	
	unsigned int length = 0x28;
	unsigned char * packet = new unsigned char[length];
	int i;
	//username = QString("blahblah");
	//password = QString("pass");
	packet[0] = 0x00;
	packet[1] = 0x28;
	packet[2] = 0x06;
	packet[3] = 0xa5;		//not really
	for(i = 0; i < username.length(); i++)
		packet[i + 4] = username.toLatin1().data()[i];
	for(i = username.length() + 4; i < 16; i++)
		packet[i] = 0x00;
	for(i = 16; i < 19; i++)
		packet[i] = 0x00;
	packet[20] = response_bit;
	for(i = 21; i < 24; i++)
		packet[i] = 0x00;
	for(i = 0; i < password.length(); i++)
		packet[24 + i] = password.toLatin1().data()[i];
	for(i = 24 + password.length(); i < length; i++)
		packet[i] = 0x00;
	
	//before encode
	printf("Sending login: ");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode_offset = 0;	//clear for new server
	encode(packet, 0x8);
	//after encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");

	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending login");
	delete[] packet;
}

void TygemConnection::handleLogin(unsigned char * msg, unsigned int length)
{
	unsigned char * p = msg;

	//0038 0698 0100 0200 228b ae64 0000 0000
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 7065 7465 7269 7573
	//0000 0002 0001 0000
	p += 8;
	encode_offset = *(unsigned long *)p;
	printf("%8x\n", encode_offset);
	sendLoginConfirm();
	sendRequest();
	connectionState = CONNECTED;
	sendName();
	serverKeepAliveTimerID = startTimer(119000);	//not fast enough? 2 minutes I thought
							//maybe we're supposed to send something
							//else soon on login too...
	sendPlayersRequest();
	//53
	//11 ae 14 name(91) 95 53
}

void TygemConnection::sendLoginConfirm(void)
{
	unsigned int length = 8;
	unsigned char * packet = new unsigned char[length];
	int i;
	
	packet[0] = 0x00;
	packet[1] = 0x08;
	packet[2] = 0x06;
	packet[3] = 0x11;
	packet[4] = 0x00;
	packet[5] = 0x00;
	packet[6] = 0x00;
	packet[7] = 0x00;
	
	//before encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	/*encode(packet, 0x8);
	//after encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
*/
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending login confirm");
	delete[] packet;
}

//we send
//00 0C 06 AE 5C C8 E9 D4 00 E8 03 00
void TygemConnection::sendRequest(void)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	int i;
	
	packet[0] = 0x00;
	packet[1] = 0x0c;
	packet[2] = 0x06;
	packet[3] = 0xae;
	packet[4] = 0x0d;
	packet[5] = 0xf0;
	packet[6] = 0xad;
	packet[7] = 0xba;
	packet[8] = 0x0d;
	packet[9] = 0xf0;
	packet[10] = 0xad;
	packet[11] = 0xba;
	//a8a88cce
	//before encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode(packet, 1);
	//after encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending request");
	delete[] packet;
}

/* Its possible we're supposed to send only ascii or only encode here */
void TygemConnection::sendName(void)
{
	unsigned int length = 24;
	unsigned char * packet = new unsigned char[length];
	int i;
	char * our_name;
	
	packet[0] = 0x00;
	packet[1] = 0x18;
	packet[2] = 0x06;
	packet[3] = 0x91;
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 4] = our_name[i];
	for(i = strlen(our_name) + 4; i < 24; i++) 	  
		packet[i] = 0x00;
	//before encode
	printf("Sending name");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode(packet, (length / 4) - 2);
	//after encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending name");
	delete[] packet;
}

void TygemConnection::sendPlayersRequest(void)
{
	unsigned int length = 8;
	unsigned char * packet = new unsigned char[length];
	int i;
	
	packet[0] = 0x00;
	packet[1] = 0x08;
	packet[2] = 0x06;
	packet[3] = 0x14;
	packet[4] = 0x00;
	packet[5] = 0x00;
	packet[6] = 0x00;
	packet[7] = 0x00;
	
	//before encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	/*encode(packet, 0x8);
	//after encode
	for(i = 0; i < length; i++)
	printf("%02x", packet[i]);
	printf("\n");
	*/
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending players list request");
	delete[] packet;
}

void TygemConnection::sendObserversRequest(unsigned short game_number)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	int i;
	
	packet[0] = 0x00;
	packet[1] = 0x0c;
	packet[2] = 0x06;
	packet[3] = 0x65;
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	packet[6] = 0x00;
	packet[7] = 0x00;
	packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
	
	printf("observer list request:\n");
	//before encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode(packet, (length / 4) - 2);
	//after encode
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending observers request");
	delete[] packet;
}


/* FIXME, these need to be const msg refs I think */
/* They also need to be reordered in a more intuitive manner
 * after we get the IGS cruft out */
void TygemConnection::handleServerList(unsigned char * msg)
{
}

void TygemConnection::changeServer(void)
{
	if(connectionState != CONNECTED)
		return;
	if(reconnectToServer() < 0)
	{
		qDebug("User canceled");
		return;
	}
}

void TygemConnection::createRoom(void)
{
	CreateRoomDialog * createroomdialog = new CreateRoomDialog(this);
	createroomdialog->exec();	
}

/* FIXME we need error handling here, a return value, something
 * or at least we need to call the right destructors throughout,
 * and safely. */
int TygemConnection::reconnectToServer(void)
{
	bool reconnecting;
	//current_server_addr[0] = '\0';
	/* FIXME, why does the serverReconnectDialog disappear the ui
	 * elements in the server window ?!?!? */
	ServerListDialog * serverReconnectDialog = new ServerListDialog(serverList, current_server_index);
	int server_i = serverReconnectDialog->exec();
	if(server_i < 0)
		return server_i;
	if(!this)
	{
		/* FIXME, this needs to be much better.  It should be,
		 * to start with, irrelevant. */
		qDebug("Waited too long to connect");
		return -1;
	}
	current_server_index = server_i;
	strncpy(current_server_addr, serverList[server_i]->ipaddress, 16);
	/* FIXME
	 * Might be neat if we listed the server that one was currently on
	 * somewhere, like on the main window. */
	
	qDebug("Reconnecting to %s: %s...", serverList[server_i]->name.toLatin1().constData(), current_server_addr);
	ConnectionInfo * newCI = new ConnectionInfo(current_server_addr,
						    12320,
						   connectionInfo->user,
						   connectionInfo->pass,
						   connectionInfo->type);
	if(connectionState == CONNECTED)
	{
		killTimer(serverKeepAliveTimerID);
		sendDisconnectMsg();
		closeConnection(false);
		reconnecting = true;
	}
	else
		reconnecting = false;
	if(openConnection(*newCI))
	{
		qDebug("Reconnected %d", reconnecting);
		connectionState = LOGIN;
		sendLogin(reconnecting);
		fflush(stdout);
		fflush(stderr);
	}
	else
		qDebug("Can't open Connection!!");
	delete newCI;

	return 0;
}

/* Check comment in igs code, FIXME */
const PlayerListing & TygemConnection::getOurListing(void)
{
	PlayerListing * p;
	p = getDefaultRoomDispatch()->getPlayerListing(getUsername());
	return *p;
}

//change text to QString
void TygemConnection::sendPersonalChat(const PlayerListing & player, const char * text)
{
	/*unsigned int length = 42 + text.size();
	while(length % 4 != 0)
		length++;
	length += 4;
	unsigned char * packet = new unsigned char[length];
	char * our_name, * msg;
	int i;
	int ordinal;
	unsigned char qualifier;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x17;
	our_name = (char *)getUsername().toLatin1().constData();
	msg = (char *)text.toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 4] = our_name[i];
	for(i = strlen(our_name) + 4; i < 18; i++) 	  
		packet[i] = 0x00;
	packet[18] = 0x00;
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[19] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[19] = ordinal + 0x1a;
	else
		packet[19] = ordinal + 0x11;
	//this would actually be our ascii name, with the earlier
	//being the encoded version
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 20] = our_name[i];
	for(i = strlen(our_name) + 20; i < 30; i++) 	  
		packet[i] = 0x00;
	packet[30] = 0x00;
	packet[31] = 0x02;	//what is this?
	packet[32] = 0x00;
	packet[33] = 0x00;
	packet[34] = 0x00;
	packet[35] = ((strlen(msg) + 1) >> 8);
	packet[36] = (strlen(msg) + 1) & 0x00ff;
	packet[37] = 0x00;
	packet[38] = 0xff;
	packet[39] = 0xff;	//possibly implied destination username?
	packet[40] = 0x33;	//don't know, sometimes 0x32, something to do with encoding?
	for(i = 0; i < strlen(msg); i++)
		packet[41 + i] = msg[i];
	packet[41 + strlen(msg)] = 0x00;
	i = 42 + strlen(msg);
	while(i % 4 != 0)
	{
		packet[i] = 0x00;
		i++;
	}
	packet[i] = 0x00;
	packet[i + 1] = 0x00;
	packet[i + 2] = 0x00;
	packet[i + 3] = 0x00;
	
	printf("personal chat before encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode(packet, (length / 4) - 2);
	
	printf("personal chat after encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending personal chat");
	delete[] packet;*/
}

/* FIXME I think both IGS and maybe even oro have a chat echo.
 * but with tygem its particularly ugly */
void TygemConnection::sendMsg(unsigned int room_number, QString text)
{
	if(text.size() > 0x7e)
	{
		qDebug("send Message too large according to tygem standard");
		return;
	}
	unsigned int length = 74 + text.size();
	while(length % 4 != 0)
		length++;
	length += 4;
	unsigned char * packet = new unsigned char[length];
	char * our_name;
	int i;
	int ordinal;
	unsigned char qualifier;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x61;
	packet[4] = (room_number >> 8);
	packet[5] = room_number & 0x00ff;
	packet[6] = 0x00;		
	packet[7] = 0x01;	//our player?
	for(i = 8; i < 24; i++)
		packet[i] = 0x00;
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 24] = our_name[i];
	for(i = strlen(our_name) + 24; i < 38; i++) 	  
		packet[i] = 0x00;
	packet[38] = 0x00;
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[39] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[39] = ordinal + 0x1a;
	else
		packet[39] = ordinal + 0x11;
	//this would actually be our ascii name, with the earlier
	//being the encoded version
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 40] = our_name[i];
	for(i = strlen(our_name) + 40; i < 51; i++) 	  
		packet[i] = 0x00;
	packet[51] = 0x00;
	packet[52] = 0x02;	//what is this?
	for(i = 53; i < 68; i++)
		packet[i] = 0x00;
	packet[68] = (text.size() + 1) & 0x00ff;
	packet[69] = 0x00; //size cannot apparently be larger than 0x7f
	packet[70] = 0x00;
	packet[71] = 0x00;
	packet[72] = 0x33;	//don't know, sometimes 0x32, something to do with encoding?
	for(i = 0; i < text.size(); i++)
		packet[73 + i] = text.toLatin1().constData()[i];
	packet[73 + text.size()] = 0x20;		//append space, don't know why
	packet[74 + text.size()] = 0x00;
	i = 74 + text.size();
	while(i % 4 != 0)
	{
		packet[i] = 0x00;
		i++;
	}
	packet[i] = 0x00;
	packet[i + 1] = 0x00;
	packet[i + 2] = 0x00;
	packet[i + 3] = 0x00;
	
	printf("room chat before encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");

	encode(packet, (length / 4) - 2);
	
	printf("room chat after encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending room chat");
	delete[] packet;
}

void TygemConnection::sendServerChat(QString text)
{
	unsigned int length = 42 + text.size();
	while(length % 4 != 0)
		length++;
	length += 4;
	unsigned char * packet = new unsigned char[length];
	char * our_name, * msg;
	int i;
	int ordinal;
	unsigned char qualifier;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x17;
	our_name = (char *)getUsername().toLatin1().constData();
	msg = (char *)text.toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 4] = our_name[i];
	for(i = strlen(our_name) + 4; i < 18; i++) 	  
		packet[i] = 0x00;
	packet[18] = 0x00;
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[19] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[19] = ordinal + 0x1a;
	else
		packet[19] = ordinal + 0x11;
	//this would actually be our ascii name, with the earlier
	//being the encoded version
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 20] = our_name[i];
	for(i = strlen(our_name) + 20; i < 30; i++) 	  
		packet[i] = 0x00;
	packet[30] = 0x00;
	packet[31] = 0x02;	//what is this?
	packet[32] = 0x00;
	packet[33] = 0x00;
	packet[34] = 0x00;
	packet[35] = ((strlen(msg) + 1) >> 8);
	packet[36] = (strlen(msg) + 1) & 0x00ff;
	packet[37] = 0x00;
	packet[38] = 0xff;
	packet[39] = 0xff;	//possibly implied destination username?
	packet[40] = 0x33;	//don't know, sometimes 0x32, something to do with encoding?
	for(i = 0; i < strlen(msg); i++)
		packet[41 + i] = msg[i];
	packet[41 + strlen(msg)] = 0x00;
	i = 42 + strlen(msg);
	while(i % 4 != 0)
	{
		packet[i] = 0x00;
		i++;
	}
	packet[i] = 0x00;
	packet[i + 1] = 0x00;
	packet[i + 2] = 0x00;
	packet[i + 3] = 0x00;
	
	printf("Server chat before encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	
	encode(packet, (length / 4) - 2);
	
	printf("Server chat after encode\n");
	for(i = 0; i < length; i++)
		printf("%02x", packet[i]);
	printf("\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending server chat");
	delete[] packet;
}


void TygemConnection::sendSetChatMsg(unsigned short phrase_id)
{
	unsigned int length = 20;
	unsigned char *packet = new unsigned char[length];
			
	packet[0] = 0x55;
	packet[1] = 0xc3;
	packet[2] = length & 0x00ff;
	packet[3] = (length >> 8);
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = 0xff;	//probably their id if directed?
	packet[7] = 0xff;
	packet[8] = 0x01;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
	packet[12] = phrase_id & 0x00ff;
	packet[13] = (phrase_id >> 8);
	packet[14] = 0x00;
	packet[15] = 0x00;
	
	packet[16] = 0x00;
	packet[17] = 0x00;
	packet[18] = 0x00;
	packet[19] = 0x00;
	
	encode(packet, 0x14);
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending set chat msg");
	delete[] packet;
}

void TygemConnection::sendJoinRoom(const RoomListing & room, const char * password)
{
	unsigned int length = 18;
	unsigned char * packet = new unsigned char[length];
	int i;

	packet[0] = 0xde;
	packet[1] = 0x5d;
	packet[2] = 0x12;
	packet[3] = 0x00;;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = room.number & 0x00ff;
	packet[7] = (room.number >> 8);
	//password padded out to ten bytes with zeroes
	//FIXME, but password only 8 in create room???
	if(password)
	{
		for(i = 0; i < (int)strlen(password); i++)
			packet[8 + i] = password[i];
		for(i = 9; i >= (int)strlen(password); i--)
			packet[8 + i] = 0x00;
	}
	else
	{
		for(i = 8; i < 18; i++)
			packet[i] = 0x00;
	}	
	qDebug("Sending join room");
	encode(packet, 0x12);
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending join room");
	delete[] packet;
}

void TygemConnection::sendFinishObserving(unsigned short game_number)
{
	unsigned int length = 12;
	char *packet = new char[length];
	int i;

	/* We need to turn this to a join if its a room, not a game
	* otherwise bad things happen FIXME */
	
	packet[0] = 0x00;
	packet[1] = 0x0c;
	packet[2] = 0x06;
	packet[3] = 0x3d;
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	packet[6] = 0xad;	//probably for encode reasons	
	packet[7] = 0xba;	//careful invites had these as well
	packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
#ifdef RE_DEBUG
	printf("Sending finishing observe to %d ", game_number);
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode((unsigned char *)packet, (length / 4) - 2);
	printf("After encode\n");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
	
	qDebug("Sending finish observing");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending finish observing");
	delete[] packet;
	//setRoomNumber(0);
	observing_game_count--;
}

/* I get the feeling that this requests the room,
 * maybe we can specify password or name if we were creating
 * it from button rather than auto on match accept.  And
 * then maybe a number comes back 
 * Incidentally, if we break on this, take too long, we can get
 * a bad answer.  Or maybe our invite response is bad and there's
 * decent checking on it now.*/
void TygemConnection::sendCreateRoom(void)
{
	unsigned int length = 40;
	char *packet = new char[length];
	int i;

	/* We need to turn this to a join if its a room, not a game
	* otherwise bad things happen FIXME */
	
	packet[0] = 0x00;
	packet[1] = 0x28;
	packet[2] = 0x06;
	packet[3] = 0x36;
	for(i = 4; i < length; i++)
		packet[i] = 0x00;
#ifdef RE_DEBUG
	printf("Sending create room");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode((unsigned char *)packet, (length / 4) - 2);
	printf("After encode\n");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", (unsigned char)packet[i]);
	printf("\n");
	
	qDebug("Sending create room");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending create room");
	delete[] packet;
	//setRoomNumber(0);	
}

void TygemConnection::sendCreateRoom(RoomCreate * room)
{
	unsigned int length = 64;
	char *packet = new char[length];
	int i;
	
	if(!room)
	{
		qDebug("CreateRoom called on NULL room!!");
		return;
	}
	if((room->title && strlen((const char *)room->title) > 20) ||
	   (room->password && strlen((const char *)room->password) > 8))
	{
		qDebug("Can't create room, bad title or password");
		return;
	}
	packet[0] = 0xca;
	packet[1] = 0x5d;
	packet[2] = 0x40;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = 0x00;
	packet[7] = 0x00;
	packet[8] = 0x00;
	packet[9] = 0x00;
	
	
	packet[10] = (room->title ? 0x80 : 0x00) | (room->password ? 0x40 : 0x00);
	//packet[10] = 0xc0; //80 open	//stronger quick 1:1 general password
	
	//set 0 before hand
	packet[11] = 0x00;
	packet[12] = 0x00;
	packet[13] = 0x00;
	packet[14] = 0x00;
	packet[15] = 0x00;
	packet[16] = 0x00;
	switch(room->type)
	{
		/* gomoku appears to use settings from game tab
		* what's the difference?  it automatically starts when
		* someone joins? */
		//gomoku equal player standard time
		//00 09 11
		case RoomCreate::GOMOKU:
		case RoomCreate::GAME:
			if(room->type == RoomCreate::GOMOKU)
				packet[11] = 0x09;
			else
				packet[11] = 0x00;
				packet[12] = ((unsigned char)room->opponentStrength << 4) |
				     	      (unsigned char)room->timeLimit;
			
			break;
		case RoomCreate::CHAT:
			packet[11] = 0xc0 | (unsigned char)room->topic;
			packet[12] = 0x00 | (unsigned char)room->location;
			//chat c1 21 movies -9 china
			//chat c0 c0 baduk all ages korea
			//chat cf 08 etc 70+ etc
			//chat c5 a3 manga 40-49 thai
			//chat c6 c4 humour 50-59 US
			qDebug("Unsupported Room Type");
			return;
			break;
		case RoomCreate::REVIEW:
			packet[11] = 0x80;
			
			packet[14] = 0x02;	//live //00 teaching
			//below 10k live commentary review
			//00 80 00 00 02 10
			//below 11k teaching focused
			//00 80 00 00 00 11 04
			//below 6d live
			//00 80 00 00 02 01
			qDebug("Unsupported Room Type");
			return;
			break;
		case RoomCreate::MULTI:
			//multi must be below or at(doublecheck?) your rank
			//multi 7 20m 20m below 10k
			//00 80 00 07 03 10 02
			//multi 6 15m 30m below 13k
			//00 80 00 06 04 13 01
			qDebug("Unsupported Room Type");
			return;
			break;
		case RoomCreate::VARIATION:
			qDebug("Unsupported Room Type");
			return;
			break;	
	}
	
	for(i = 17; i < 26; i++)
		packet[i] = 0x00;
	//title padded to 20
	if(room->title)
	{
		for(i = 0; i < (int)strlen((const char *)room->title); i++)
			packet[26 + i] = room->title[i];
		for(i = 19; i >= (int)strlen((const char *)room->title); i--)
			packet[26 + i] = 0x00;
	}
	else
	{
		for(i = 26; i < 46; i++)
			packet[i] = 0x00;
	}	
	for(i = 46; i < 56; i++)
		packet[i] = 0x00;
	//password padded to 8
	if(room->password)
	{
		for(i = 0; i < (int)strlen((const char *)room->password); i++)
			packet[56 + i] = room->password[i];
		for(i = 7; i >= (int)strlen((const char *)room->password); i--)
			packet[56 + i] = 0x00;
	}
	else
	{
		for(i = 56; i < 64; i++)
			packet[i] = 0x00;
	}	
	
	encode((unsigned char *)packet, 0x1a);
	qDebug("Sending create room");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending create room");
	delete[] packet;
	//setRoomNumber(0);
	delete room;
}

/* Tygem has a different system.  We send a basic invite and if
 * its accepted we enter a board room where there is then the debate
 * on match specifics. */
void TygemConnection::sendMatchInvite(const PlayerListing & player)
{
	sendMatchInvite(player, offer);
}

/* note that this only enters the match offer dialog
 * it doesn't accept the game.  Haven't decided whether
 * to auto send this or to have a special popup for it.*/
void TygemConnection::sendMatchInvite(const PlayerListing & player, enum MIVersion version, unsigned short room_number)
{
	unsigned int length = 0x44;
	unsigned char * packet = new unsigned char[length];
	char * our_name, * their_name;
	int ordinal;
	char qualifier;
	int i;
	if(version == offer)
		opponent_is_challenger = false;
	//else we actually might want to set it here instead of in handleMatchInvite
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	if(version == accept || version == decline)
		packet[3] = 0x44;
	else	//offer or create
		packet[3] = 0x43;
	/* Below is similar to sendServerChat, there should be some
	 * object for creating these records automatically, like addPlayerToHeader
	 * FIXME */
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	/* Careful, they could have logged off in the mean time possibly? */
	their_name = (char *)player.name.toLatin1().constData();
	
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 4] = their_name[i];
	for(i = strlen(their_name) + 4; i < 18; i++) 	  
		packet[i] = 0x00;
	packet[18] = 0x00;
	if(version == accept)
	{
		sscanf(player.rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
		if(qualifier == 'k')
			packet[19] = 0x12 - ordinal;
		else if(qualifier == 'p')
			packet[19] = ordinal + 0x1a;
		else
			packet[19] = ordinal + 0x11;
		/*packet[22] = 0x00;
		for(i = 0; i < strlen(player.rank.toLatin1().constData()); i++)
			packet[i + 20] = player.rank.toLatin1().constData()[i];
		for(i = 23; i < 31; i++)
			packet[i] = 0x00;*/
		their_name = (char *)player.ascii_name.toLatin1().constData();
		for(i = 0; i < strlen(their_name); i++)
			packet[i + 20] = their_name[i];
		for(i = strlen(their_name) + 20; i < 30; i++) 	  
			packet[i] = 0x00;
		packet[30] = 0x00;
		//thinking this is supposed to be opposite of 19 from offer/invite
		packet[31] = 0x02;
		//packet[31] = 0x00;
		//packet[31] = 0x01;	//this may match 59 from offer/invite from them
		//packet[31] = (int)opponent_is_challenger + 1;
		//packet[31] = FIRST_BYTE(mi_flags);
		//packet[31] = 0x01;	//maybe for "second accepting" player?
	}
	else if(version == decline)
	{
		sscanf(player.rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
		if(qualifier == 'k')
			packet[19] = 0x12 - ordinal;
		else if(qualifier == 'p')
			packet[19] = ordinal + 0x1a;
		else
			packet[19] = ordinal + 0x11;
		for(i = 20; i < 32; i++)
			packet[i] = 0x00;
	}
	else if(version == offer)
	{
		/* Changing 19 from 01 to 02 in our offer changes 
		/* 47 from 01 to 02 in the decline */
		/* In addition, when challenging a stronger player, I found
		 * this to be 0, not 1 or 2 !! */
		//testing rank
		//packet[19] = 0x02;
		packet[19] = 0x00;	//what is this (doublecheck asciiishness)
		their_name = (char *)player.ascii_name.toLatin1().constData();
		for(i = 0; i < strlen(their_name); i++)
			packet[i + 20] = their_name[i];
		for(i = strlen(their_name) + 20; i < 32; i++) 	  
			packet[i] = 0x00;
	}
	else if(version == create)
	{
		for(i = 19; i < 32; i++)
			packet[i] = 0x00;
	}
	/* The sscanfs above, like to kill this our_name.  Probably
	 * something about why I shouldn't cast char *s and const, something, 
	 * something... anyway FIXME */
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 32] = our_name[i];
	for(i = strlen(our_name) + 32; i < 46; i++) 	  
		packet[i] = 0x00;
	packet[46] = 0x00;
	if(version == accept || version == decline)
	{
		//if(version == decline)
		//packet[47] = 0x00;
			//packet[47] = 0x01;	//they're first player
			//packet[47] = SECOND_BYTE(mi_flags);
		//should match 19 from invite !!!
		//packet[47] = 0x01;	//this to match 19 in invite?
		//packet[47] = invite_byte;
		packet[47] = 0x00;
		//presumably this is opposite of 31 which is same as 59
		//from invite offer
	}
	else// if(version == offer || version == create)
	{
		sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
		if(qualifier == 'k')
			packet[47] = 0x12 - ordinal;
		else if(qualifier == 'p')
			packet[47] = ordinal + 0x1a;
		else
			packet[47] = ordinal + 0x11;
	}
	/*FIXME, this should be unnecessary, but above sscanf might
    	 * have randomly killed it, turned it into rank */
	our_name = (char *)getUsername().toLatin1().constData();
	//this would actually be our ascii name, with the earlier
	//being the encoded version
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 48] = our_name[i];
	for(i = strlen(our_name) + 48; i < 58; i++) 	  
		packet[i] = 0x00;
	packet[58] = 0x00;
	if(version == accept)
		packet[59] = 0x02;	//this might be matched with person offering? FIXME
	else if(version == offer)
	{
		packet[59] = 0x02;
		invite_byte = packet[59];
	}
	/* This byte on our offer looks echoed after second "their name", really our
	 * name on the responding accept, right before their_name, "our_name" in the code */
	//packet[59] = THIRD_BYTE(mi_flags);
	if(version == accept)
	{
		packet[60] = 0x01;
		packet[61] = 0x01;
		//packet[61] = FOURTH_BYTE(mi_flags);
		packet[62] = 0x00;
		packet[63] = 0x00;
	}
	else if(version == offer || version == decline)
	{
		packet[60] = 0x01;
		packet[61] = 0x00;
		packet[62] = 0xff;
		packet[63] = 0xff;	//not implied username... see server chat comment
	}
	else if(version == create)
	{
		packet[60] = 0x02;
		packet[61] = 0x00;
		/* Mr Wang to Niu Yen:
		 * I know, let's randomly change the byte order just on this one 
		 * piece of information, just to screw up people trying to reverse
		 * the protocol: */
		packet[62] = room_number & 0x00ff;
		packet[63] = (room_number >> 8);
		/* No no, they're probably just inconsistent with their ntohs calls... */
	}
	for(i = 64; i < 68; i++)
		packet[i] = 0x00;
	
	//00 44 06 44 69 6e 74 72 75 73 69 6f 6e 00 00 00 00 00 
	//00 09 69 6e 74 72 75 73 69 6f 6e 00 00 01 70 65 74 65 72 69 75 73 00 00 00 00 00 00 00 01 7
	//0 65 74 65 72 69 75 73 00 00 00 02 01 02 00 00 00 00 00 00
#ifdef RE_DEBUG
	printf("match %s packet before encoding: ", (version == offer ? "offer" :
						     (version == accept ? "accept" :
						     (version == decline ? "decline" : "create"))));
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, (length / 4) - 2);
	qDebug("Sending accept match request or invite");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending accept match request or invite");
	delete[] packet;
}

/* This looks like its a declaration of the match for the game listings */
void TygemConnection::sendMatchMsg1(const PlayerListing & player, unsigned short game_number)
{
	unsigned int length = 0x2c;
	unsigned char * packet = new unsigned char[length];
	char * our_name, * their_name;
	int ordinal;
	char qualifier;
	int i;
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0xc6;
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	packet[6] = 0x00;
	packet[7] = 0x00;
	
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	our_name = (char *)getUsername().toLatin1().constData();
	/* Careful, they could have logged off in the mean time possibly? */
	
	
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 8] = our_name[i];
	for(i = strlen(our_name) + 8; i < 22; i++) 	  
		packet[i] = 0x00;
	packet[22] = 0x00;
	/* If this is our rank, we may have made mistakes elsewhere FIXME,
	 * its right before their name, but its after our name */
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[23] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[23] = ordinal + 0x1a;
	else
		packet[23] = ordinal + 0x11;
	
	their_name = (char *)player.name.toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 24] = their_name[i];
	for(i = strlen(their_name) + 24; i < 38; i++) 	  
		packet[i] = 0x00;
	packet[38] = 0x00;
	sscanf(player.rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[39] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[39] = ordinal + 0x1a;
	else
		packet[39] = ordinal + 0x11;
	packet[40] = 0x00;
	packet[41] = 0x00;
	packet[42] = 0x00;
	packet[43] = 0x00;
	
	printf("Match msg 1: \n");
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
	encode(packet, (length / 4) - 2);
	qDebug("Sending match msg 1");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending match msg 1");
	delete[] packet;	
}

void TygemConnection::sendMatchOffer(const MatchRequest & mr, enum MIVersion version)
{
	unsigned int length = 0x5c;
	unsigned char * packet = new unsigned char[length];
	int ordinal;
	char qualifier;
	char * our_name, * their_name;
	int i;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	PlayerListing * opponent = roomdispatch->getPlayerListing(mr.opponent);
	if(!opponent)
	{
		qDebug("Can't get opponent listing for match request\n");
		return;
	}
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	if(version == offer)
		packet[3] = 0x45;
	else if(version == accept)
		packet[3] = 0x46;
	//their name first
	their_name = (char *)opponent->name.toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 4] = their_name[i];
	for(i = strlen(their_name) + 4; i < 18; i++) 	  
		packet[i] = 0x00;
	packet[18] = 0x00;
	sscanf(opponent->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[19] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[19] = ordinal + 0x1a;
	else
		packet[19] = ordinal + 0x11;
	
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 20] = our_name[i];
	for(i = strlen(our_name) + 20; i < 34; i++) 	  
		packet[i] = 0x00;
	packet[34] = 0x00;
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[35] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[35] = ordinal + 0x1a;
	else
		packet[35] = ordinal + 0x11;
	packet[36] = (mr.number >> 8);
	packet[37] = mr.number & 0x00ff;
	//012c 2801 0100 0100 0002	//offer?
	//012c 2801 0100 0100 0001	//accept?
	//0bb8 2803 0100 0100 0002	//offer
	//time settings
	//0007 012c 1e03 0100 0100
	if(version == acknowledge)
	{
		for(i = 38; i < 47; i++)
			packet[i] = 0x00;
	}
	else
	{
		/*I've seen an accept sent with these as 0s
		 * and yet that literally sets time to 0 so... */
		//if(version == offer)
		//{
			packet[38] = (mr.maintime >> 8);
			packet[39] = mr.maintime & 0x00ff;
			packet[40] = mr.periodtime;
			packet[41] = mr.stones_periods;
			packet[42] = mr.handicap;	//this is handicap bit!!!
			packet[43] = 0x00;
		/*}
		else if(version == accept)
		{
			for(i = 38; i < 44; i++)
				packet[i] = 0x00;
		}*/
		switch(mr.color_request)
		{
			case MatchRequest::WHITE:
				if(version == offer)
					packet[44] = 0x01;
				else
					packet[44] = 0x00;
				break;
			case MatchRequest::BLACK:
				if(version == offer)     //their color when we offer?
					packet[44] = 0x00;
				else
					packet[44] = 0x01;//our color when we accept
				break;
			case MatchRequest::NIGIRI:
				packet[44] = 0x02;
				break;	
		
		}
		printf("*** match offer packet 44 should be 1: %02x\n", packet[44]);
		//FIXME
		/* This bit, if we're offering is basically ignored since
		 * sent 0671 sendStartGame overrides colors, etc. */
		//if(version == accept)
		//	packet[44] = 0x00;	//definitely we're black in tests right now
		/* Changing the accept above from 01 to 00 fixed the names but broke our move
		 * ability as white */
		/* On the other hand, changing the offer from 01 to 00 screwed the names
		 * but allows us to play white */
		/* Trying both as 01 with 01 below instead of 02 after our name */
		/* That last 02 to 01 broke our move ability, so thinking, that's
		 * offerer's color? */
		//02 seemed to do something strange
					//maybe swapped the pictures but not the names
		/* For 44, 0x00 means that, for this as an accept send,
		 * the challenger, that first name, is black, and we're white
		 * 0x01 probably means the opposite, 02 might be nigiri or something
		 * but there's flags later, that also need to be set I think */
		/* Actually 02 instantly makes challenger black and allow them to play...
		 * but then looks like inconsistent, so yeah, nigiri */
		//00 here is, as accept send, we play white, they play black,
		//according to bits below and name orders as they are, this seems
		// to work except we can't play
		//packet[45] = 0x00;
		//packet[46] = 0x00;
		packet[45] = 0x01;
		packet[46] = 0x01;
		/* These two bytes are likely about non rated games... or they're
		 * the ones we're looking for */
		//packet[46] = SECOND_BYTE(mo_flags);
	}
	if(version == offer)
	{
		packet[47] = 0x02;	//offer bit
		packet[48] = 0x00;
		packet[49] = 0x00;
		packet[50] = 0x00;
		packet[51] = 0x00;
	}
	else if(version == accept)
	{
		packet[47] = 0x01;	//accept bit
		/* Could also be challenger bit, but if we change 47 to 02
		 * it definitely rechallenges or disputes or something */
		packet[48] = 0xff;
		packet[49] = 0xff;
		packet[50] = 0xff;
		packet[51] = 0xff;
	}
	else if(version == acknowledge)
	{
		packet[47] = 0x03;
		packet[48] = 0xff;
		packet[49] = 0xff;
		packet[50] = 0xff;
		packet[51] = 0xff;
	}
	
	if(version == offer)
	{
		for(i = 52; i < 76; i++)
			packet[i] = 0x00;
	}
	else if(version == accept || version == acknowledge)
	{
		for(i = 52; i < 56; i++)
			packet[i] = 0x00;
		packet[56] = 0x00;
		packet[57] = ZODIAC_BYTE;
		packet[58] = 0x00;
		packet[59] = 0x00;	//their pic?
		for(i = 60; i < 64; i++)
			packet[i] = 0x00;
		//actually our name? FIXME
		//their_name = (char *)getUsername().toLatin1().constData();
		their_name = (char *)opponent->name.toLatin1().constData();;
		for(i = 0; i < strlen(their_name); i++)
			packet[i + 64] = their_name[i];
		for(i = strlen(their_name) + 64; i < 74; i++) 	  
			packet[i] = 0x00;
		packet[74] = 0x00;
		
		packet[75] = 0x01;	//what is this? FIXME affects names?
		//packet[75] = THIRD_BYTE(mo_flags);
		//75, as 0x02 makes our name first as white
		//if 87 is also 0x02 later which makes opponent second black
		//01 makes the later name whites name
		//02 makes the later name blacks name
		//(was with 01 later)
		//send 0 1, with 0 here, their name
		//send 0 1, with 1 here, their name
	}
	//02 01 with 00 earlier lets challenger be black
	//01 02 with 01 earlier seems screwed up
	//01 02 with 00 earlier makes us white and challenger black
	//01 02 with 02 earlier screwed up, we're black but can't play
	//02 01 with 02 earlier lets challenger play black but we can't play
	//02 01 with 01 earlier we seem to be able to play as black but they have cursor
				//and can't
	//02 02 with 01 earlier we seem to be able to play as black but they have cursor
				//and can't
	//00 00 with 01 earlier, same
	//00 00 with 02 earlier, same
	//00 00 with 00 earlier, challenger plays black, okay, moves screwy
	//02 00 with 00 earlier, same
	//01 00 with 00 earlier, same
	//02 02 with 00 earlier, same
	//01 02 with 01 earlier, challenger plays white, we maybe can play but doesn't
				//go through
	if(version == accept)
	{
		//our_name = (char *)opponent->name.toLatin1().constData();
		our_name = (char *)getUsername().toLatin1().constData();
	}
	else
	{
		our_name = (char *)getUsername().toLatin1().constData();
	}
	

	for(i = 0; i < strlen(our_name); i++)
		packet[i + 76] = our_name[i];
	for(i = strlen(our_name) + 76; i < 86; i++) 	  
		packet[i] = 0x00;
	packet[86] = 0x00;
	//per match invite, I don't think we can change this...
	if(version == offer)
		packet[87] = 0x01;
	else if(version == accept || version == acknowledge)
		packet[87] = 0x02;		//invite_byte?
	//just changed from 02
	//presumably matches with match invites, etc.
	//packet[87] = FOURTH_BYTE(mo_flags);
	//02 here seems to set this name as the white player
	//01 here with 02 above seems to scre up this name as 
	//player...
	for(i = 88; i < 92; i++)
		packet[i] = 0x00;
	//005c 0645 696e 7472 7573 696f 6e00 0000
	//0000 0009 7065 7465 7269 7573 0000 0000
	//0000 0008 00ab 012c 2801 0100 0100 0002
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 0000 0000 7065 7465
	//7269 7573 0000 0002 0000 0000
	
	//what we're sent from intrusion when intrusion offers
	//7065 7465 7269 7573 0000 0000 0000 0008
	//696e 7472 7573 696f 6e00 0000 0000 0009
	//00ab 0bb8 2803 0100 0100 0002 0000 0000
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 696e 7472 7573 696f
	//6e00 0002
	
	//we are peterius sending this
	//005c0645
	//696e 7472 7573 696f 6e00 0000 0000 0009
	//7065 7465 7269 7573 0000 0000 0000 0008
	//0011 012c 2801 0100 0100 0002 0000 0000
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 7065 7465 7269 7573
	//0000 0002 0000 0000
	playing_game_number = mr.number;		//so the match can bring it up
	//005c 0645 696e 7472 7573 696f 6e00 0000
	//0000 0009 7065 7465 7269 7573 0000 0000
	//0000 0008 0000 0258 5819 0000 0100 0002
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 0000 0000 7065 7465
	//7269 7573 0000 0002 0000 0000 
	
#ifdef RE_DEBUG
	printf("match packet we are sending now: ");
	for(i = 0; i < (int)length; i++)
		printf("%02x", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	
	encode(packet, (length / 4) - 2);
	qDebug("Sending match offer");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending match offer");
	delete[] packet;
	return;
}

void TygemConnection::sendStartGame(const MatchRequest & mr)
{
	unsigned int length = 0x60;
	unsigned char * packet = new unsigned char[length];
	int ordinal;
	char qualifier;
	char * our_name, * their_name;
	int i;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * ourPlayer;
	PlayerListing * opponent = roomdispatch->getPlayerListing(mr.opponent);
	if(!opponent)
	{
		qDebug("Can't get opponent listing for match request\n");
		return;
	}
	ourPlayer = roomdispatch->getPlayerListing(getUsername());
	if(!ourPlayer)
	{
		qDebug("Can't get our player listing");
		return;
	}
	/* To start with, we'll craft and send, and then we'll work out
	* details later */
	
	
	/* Careful, they could have logged off in the mean time possibly? */
	
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x71;
	packet[4] = (mr.number >> 8);
	packet[5] = mr.number & 0x00ff;
	packet[6] = 0x00;
	packet[7] = 0x01;	//probably color
	//their name first
	their_name = (char *)opponent->name.toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 8] = their_name[i];
	for(i = strlen(their_name) + 8; i < 22; i++) 	  
		packet[i] = 0x00;
	packet[22] = 0x00;
	sscanf(opponent->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[23] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[23] = ordinal + 0x1a;
	else
		packet[23] = ordinal + 0x11;
	
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 24] = our_name[i];
	for(i = strlen(our_name) + 24; i < 38; i++) 	  
		packet[i] = 0x00;
	packet[38] = 0x00;
	sscanf(ourPlayer->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[39] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[39] = ordinal + 0x1a;
	else
		packet[39] = ordinal + 0x11;
	
	//012c 2801 0100 0100 0002
	//time settings
	packet[40] = (mr.maintime >> 8);
	packet[41] = mr.maintime & 0x00ff;
	packet[42] = mr.periodtime;
	packet[43] = mr.stones_periods;
	packet[44] = mr.handicap;
	//for now since time isn't really set right
	packet[40] = 0x01;	//04	//time setting type?
	packet[41] = 0x2c;	//b0	//minutes?
	packet[42] = 0x28;	//1e	//seconds?
	packet[43] = 0x01;	//03	//periods?
	//packet[44] = 0x01;
	
	packet[45] = 0x00;
	//this is our color
	/* We appear to be able to change this in the games we
	 * send with impunity */
	switch(mr.color_request)
	{
		case MatchRequest::WHITE:
			packet[46] = 0x00;
			break;
		case MatchRequest::BLACK:
			packet[46] = 0x01;
			break;
		case MatchRequest::NIGIRI:
			packet[46] = 0x02;
			break;	
		
	}
	qDebug("packet 46: %02x", packet[46]);
	//packet[46] = 0x00;	//our color 1 = white
	packet[47] = 0x00;
	
	packet[48] = 0x01;
	packet[49] = 0x00;
	packet[50] = 0x00;
	packet[51] = 0x12;
	
	packet[52] = 0xff;
	packet[53] = 0xff;
	packet[54] = 0xff;
	packet[55] = 0xff;
	
	packet[56] = 0xff;
	packet[57] = 0xff;
	packet[58] = 0xff;
	packet[59] = 0xff;
	
	packet[60] = 0x00;
	packet[61] = 0x01;		//pics?
	packet[62] = 0x00;
	packet[63] = ZODIAC_BYTE;
	//proper order here seems to be their name then our name
	//careful might change if we play black
	//our_name = (char *)getUsername().toLatin1().constData();
	our_name = (char *)opponent->name.toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 64] = our_name[i];
	for(i = strlen(our_name) + 64; i < 74; i++) 	  
		packet[i] = 0x00;
	packet[74] = 0x00;
	packet[75] = 0x02;      //changed from 2 in the hope of making opp black
	//their_name = (char *)opponent->name.toLatin1().constData();
	their_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 75] = their_name[i];
	for(i = strlen(their_name) + 75; i < 86; i++) 	  
		packet[i] = 0x00;
	packet[86] = 0x00;
	packet[87] = 0x02;		
	for(i = 88; i < 96; i++)
		packet[i] = 0x00;
	
#ifdef RE_DEBUG
	printf("start match we are sending now: ");
	for(i = 0; i < (int)length; i++)
		printf("%02x", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	
	encode(packet, (length / 4) - 2);
	qDebug("Sending start match");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending start match");
	delete[] packet;
	return;
}

void TygemConnection::sendGameUpdate(unsigned short game_code)
{
	unsigned int length = 0xa;
	unsigned char * packet = new unsigned char[length];
	packet[0] = 0x50;
	packet[1] = 0xc3;
	packet[2] = 0x0a;
	packet[3] = 0x00;
	packet[4] = game_code & 0x00ff;
	packet[5] = (game_code >> 8);
	
	/* Okay looks like packet[6] controls the smiley faces ?
	 * 0b might be 0 shaped mouth, 0a could be yawn with tear? */
	packet[6] = 0x09;	//huh?? 0b
	packet[7] = 0x00;
	
	packet[8] = 0x00;
	packet[9] = 0x00;
//50 c3 0a 00 cd 01 0b 00 00 00
//50 c3 0a 00 0b 09 00 fc f6 08		//?
	encode(packet, 0x0a);
	qDebug("Sending game update");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending game update");
	delete[] packet;
}

void TygemConnection::sendServerKeepAlive(void)
{
	unsigned int length = 8;
	unsigned char * packet = new unsigned char[length];
	
	packet[0] = 0x00;
	packet[1] = 0x08;
	packet[2] = 0x06;
	packet[3] = 0x4e;
	packet[4] = 0x00;
	packet[5] = 0x00;
	packet[6] = 0x00;
	packet[7] = 0x00;
	
	qDebug("Sending server keep alive");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending server keep alive");
	delete[] packet;
}

void TygemConnection::sendKeepAlive(const GameListing & game)
{
	unsigned int length = 0x10;
	unsigned char * packet = new unsigned char[length];
	BoardDispatch * boarddispatch = getIfBoardDispatch(game.number);
	if(!boarddispatch)
	{
		qDebug("tried to sendKeepAlive on nonexistent board %d", game.number);
		return;
	}
	TimeRecord tr = boarddispatch->getOurTimeRecord();
	GameData * gr = boarddispatch->getGameData();
	packet[0] = 0x64;
	packet[1] = 0xc3;
	packet[2] = 0x10;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game.game_code & 0x00ff;
	packet[7] = (game.game_code >> 8);
	if(gr->white_name == getUsername())
		packet[8] = 0x01;	//color?
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	
	//time
	packet[10] = tr.time & 0x00ff;
	packet[11] = (tr.time >> 8);
	//periods
	packet[12] = tr.stones_periods & 0x00ff;
	packet[13] = (tr.stones_periods >> 8);
	packet[14] = 0x00;		//probably whether we're in byo yomi?
	packet[15] = 0x00;		//FIXME

	encode(packet, 0x10);
	qDebug("Sending keep alive");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending keep alive");
	delete[] packet;
}

/* Unnecessary but we don't want to raise suspicion */
void TygemConnection::sendRequestKeepAlive(const GameListing & game)
{
	unsigned int length = 0x0c;
	unsigned char * packet = new unsigned char[length];
	BoardDispatch * boarddispatch = getIfBoardDispatch(game.number);
	if(!boarddispatch)
	{
		qDebug("tried to sendRequestKeepAlive on nonexistent board %d", game.number);
		return;
	}
	TimeRecord tr = boarddispatch->getOurTimeRecord();
	GameData * gr = boarddispatch->getGameData();
	packet[0] = 0x5c;
	packet[1] = 0x4e;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game.game_code & 0x00ff;
	packet[7] = (game.game_code >> 8);
	packet[8] = gr->moves & 0x00ff;
	packet[9] = (gr->moves >> 8);
	packet[10] = 0x00;		//probably whether we're in byo yomi?
	packet[11] = 0x00;		//FIXME

	qDebug("sending request keepalive on moves: %d", gr->moves);
	encode(packet, 0x0c);
	qDebug("Sending request keep alive");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending keep alive2");
	delete[] packet;
}

void TygemConnection::timerEvent(QTimerEvent * event)
{
	if(event->timerId() == serverKeepAliveTimerID)
		sendServerKeepAlive();
	/*if(event->timerId() == matchKeepAliveTimerID ||
		  event->timerId() == matchRequestKeepAliveTimerID)
	{
		RoomDispatch * roomdispatch = getDefaultRoomDispatch();
		GameListing * listing = roomdispatch->getGameListing(our_game_being_played);
		if(!listing)
		{
			qDebug("Can't send keepalive, no listing");
			return;
		}
		if(event->timerId() == matchKeepAliveTimerID)
			sendKeepAlive(*listing);
		else
			sendRequestKeepAlive(*listing);
	}*/
}

void TygemConnection::sendTime(BoardDispatch * boarddispatch)
{
	unsigned int length = 0x14;
	unsigned char * packet = new unsigned char[length];
	GameData * gd = boarddispatch->getGameData();
	TimeRecord us = boarddispatch->getOurTimeRecord();
	TimeRecord them = boarddispatch->getTheirTimeRecord();
	int i;
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x7d;
	packet[4] = (gd->number >> 8);
	packet[5] = gd->number & 0x00ff;
	//we're black sending this
	//            f   p s  m
	//0101 013b 0440 0100 0580 0000 0000
	packet[6] = (gd->black_name != getUsername()) ^ gd->white_first_flag;
	//packet[6] = (gd->black_name == getUsername());
	packet[7] = 0x01;
	if(us.stones_periods == -1)
		packet[8] = gd->stones_periods;
	else
		packet[8] = us.stones_periods;
	packet[9] = us.time % 60;
	packet[10] = (us.time - packet[9]) / 60;
	//0x40 and 0x80 are also used for something as
	//well as a time loss flag as well
	if(gd->black_name == getUsername())
		packet[11] = 0x40;
	else
		packet[11] = 0x80;
	if(us.stones_periods != -1)
		packet[11] |= 0x20
				;
	if(them.stones_periods == -1)
		packet[12] = gd->stones_periods;
	else
		packet[12] = them.stones_periods;
	packet[13] = them.time % 60;
	packet[14] = (them.time - packet[13]) / 60;
	if(gd->black_name == getUsername())
		packet[15] = 0x80;
	else
		packet[15] = 0x40;
	if(them.stones_periods != -1)
		packet[15] |= 0x20;
	packet[16] = 0x00;
	packet[17] = 0x00;
	packet[18] = 0x00;
	packet[19] = 0x00;
	printf("TIME SENT: ");
	for(i = 0; i < 20; i++)
		printf("%02x ", packet[i]);
	printf("\n");
	encode(packet, (length / 4) - 2);
	
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending time");
	delete[] packet;
	return;
}
//their first move as black
//0 STO 0 2 1 15 15
//0 SUR 0 3 1 //their surrender after our bad 2nd move
void TygemConnection::sendMove(unsigned int game_id, MoveRecord * move)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_id);
	if(!boarddispatch)
	{
		qDebug("Can't get board for send move: %d", game_id);
	}
	GameData * r = boarddispatch->getGameData();
	/*switch(move->flags)
	{
		case MoveRecord::RESIGN:
			sendResign(r->game_code);
			killActiveMatchTimers();
			return;
			break;
			
		
		case MoveRecord::UNREMOVE:
			sendRemoveStones(r->game_code, move);
			return;	
			break;
		case MoveRecord::DONE_SCORING:
			sendDoneScoring(r->game_code, r->opponent_id);
			return;	
			break;
		case MoveRecord::REQUESTUNDO:
			sendUndo(r->game_code, move);
			return;
			break;
		case MoveRecord::UNDO:
			sendAcceptUndo(r->game_code, move);
			return;
			break;
		case MoveRecord::REFUSEUNDO:
			sendDeclineUndo(r->game_code, move);
			return;
			break;
		default:
			break;
	}*/
	
	
	char move_str[32];
	int player_number;
	int i;
	static bool previous_move_pass = false;
	if(move->number + 2 > move_message_number)
		move_message_number = move->number + 2;
	else
		move_message_number++;
	//0001	//color
	//0000 0000 0000 ADBA	//probably like 8 zeroes
	//text string ending in 200a and 0 padded out to the full 32
	//like:
	//SUR 0 2 2		white surrenders
	//STO 0 move_number player_number x y 0a
	//SUR 0 move_number player_number 0a
	//REM 0 312 -1 -1 0 1 
	//REM 0 307 5 6 1 0 
	//REM 0 306 10 14 2 0 			un remove?
	//REM 0 number x y player_number 0
	//DSC 1 0 1			//possibly disagreement?
	//DSC 1 0 2 
	
	//BAC 0 246 1 	//part of review
	//CSP 0 233 0
	//CST 0 234 
	//FOR 0 186 1
	/* Might also just be stone color here without the flag */
	//player_number = ((r->black_name == getUsername()) ^ r->white_first_flag) + 1;
	//1 is likely black, 2, white
	player_number = (r->black_name != getUsername()) + 1;
	//player_number = SECOND_BYTE(send_flags);
	switch(move->flags)
	{
		case MoveRecord::RESIGN:
			//if this is resign, we also have to 
			//send the 0670 or 0672 or whatever if its our offer/game
			sprintf(move_str, "SUR %d %d %d \n", 0, move_message_number, player_number); 
			break;
		case MoveRecord::DONE_SCORING:
			sprintf(move_str, "REM %d %d -1 -1 %d %d \n", 0, move_message_number, player_number, 0 );
			break;
		case MoveRecord::REMOVE:
			//move number continues here ! FIXME
			sprintf(move_str, "REM %d %d %d %d %d %d \n", 0, move_message_number, move->x - 1, move->y - 1, player_number, 0 );
			break;
		case MoveRecord::PASS:
			sprintf(move_str, "SKI %d %d \n", 0, move_message_number);
			break;
		case MoveRecord::NONE:
		default:
			sprintf(move_str, "STO %d %d %d %d %d \n", 0, move_message_number, player_number, move->x - 1, move->y - 1);
			break;
	}
	unsigned int length = 16 + strlen(move_str);
	while(length % 4 != 0)
		length++;
	length += 4;
	unsigned char * packet = new unsigned char[length];
	
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x68;
	packet[4] = (r->number >> 8);
	packet[5] = r->number & 0x00ff;
	//packet[6] = (r->black_name != getUsername()) ^ r->white_first_flag;
	packet[6] = 0x01;
	//packet[6] = FIRST_BYTE(send_flags);
	packet[7] = 0x01;
	for(i = 8; i < 16; i++)
		packet[i] = 0x00;
	
	strncpy((char *)&(packet[16]), move_str, strlen(move_str));
	for(i = strlen(move_str) + 16; i < length; i++)
		packet[i] = 0x00;

#ifdef RE_DEBUG
	printf("movepacket: ");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
	for(i = 0; i < (int)length; i++)
		printf("%c", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, (length / 4) - 2);
	qDebug("Sending move");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending move packet");
	delete[] packet;
	
	if(move->flags == MoveRecord::RESIGN)
	{
		//FIXME, pretty sure this is just if its our game
		//actually maybe loser does always send this
		sendMatchResult(r->number);
	}
	else if(move->flags == MoveRecord::PASS)
	{
		if(previous_move_pass && previous_opponent_move_pass)
		{
			sendEnterScoring(r->number);
		}
		else
			previous_move_pass = true;
	}
	else
	{
		//don't need to set this in resign?
		previous_move_pass = false;
	}
}

void TygemConnection::sendUndo(unsigned int game_code, const MoveRecord * move)
{
	unsigned int length = 0x0e;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us an undo");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xe6;
	packet[1] = 0xaf;
	packet[2] = 0x0e;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = move->number & 0x00ff;	//this is the move to start from (10)
	packet[11] = (move->number >> 8);
	packet[12] = 0x00;
	packet[13] = 0x00;
	
//from sender perspective	encode(e)
//e6af 0e00 b50d 4c02 01 00 0a 00 00 00
#ifdef RE_DEBUG
	printf("undo packet: ");
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, 0xe);
	qDebug("Sending undo");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending undo packet");
	delete[] packet;
}

/* FIXME oro client can't seem to play after an undo, but we can if
 * its our turn.  We need to make sure that this is an ORO bug, and not
 * us screwing up the move number we send back */
void TygemConnection::sendDeclineUndo(unsigned int game_code, const MoveRecord * move)
{
	unsigned int length = 14;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us a timeloss");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xf0;
	packet[1] = 0xaf;
	packet[2] = 0x0e;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	if(aGameData->white_name == getUsername())	//opposite color
		packet[8] = 0x00;
	else
		packet[8] = 0x01;
	packet[9] = 0x00;
	packet[10] = move->number & 0x00ff;
	packet[11] = (move->number >> 8);
	packet[12] = 0x00;
	packet[13] = 0x00;
	
	encode(packet, 0x0e);
	qDebug("Sending decline undo");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending decline undo");
	delete[] packet;
}

void TygemConnection::sendAcceptUndo(unsigned int game_code, const MoveRecord * move)
{
	unsigned int length = 14;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us a timeloss");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xeb;
	packet[1] = 0xaf;
	packet[2] = 0x0e;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = move->number & 0x00ff;
	packet[11] = (move->number >> 8);
	packet[12] = 0x00;
	packet[13] = 0x00;
	
	encode(packet, 0x0e);
	qDebug("Sending accept undo");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending accept undo");
	delete[] packet;
}

void TygemConnection::sendTimeLoss(unsigned int game_id)
{
	return;
	//FIXME
	unsigned int length = 0x0c;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_id);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us a timeloss");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xf6;
	packet[1] = 0xb3;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = aGameData->game_code & 0x00ff;
	packet[7] = (aGameData->game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;

	encode(packet, 0xc);
	qDebug("Sending time loss");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending timeloss packet");
	delete[] packet;
	killActiveMatchTimers();
}

void TygemConnection::sendRemoveStones(unsigned int game_code, const MoveRecord * move)
{
	unsigned int length = 0x10;
	unsigned char * packet = new unsigned char[length];
	int i;
	
	packet[0] = 0xe2;
	packet[1] = 0xb3;
	packet[2] = 0x10;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	packet[8] = our_player_id & 0x00ff;	//why our id again??
	packet[9] = (our_player_id >> 8);
	//packet[10] = (move.number & 0x00ff);		//what the hell are these?
	//packet[11] = (move.number >> 8);		// FIXME scores???
	if(move->flags == MoveRecord::REMOVE)
		packet[12] = 0x01;		//probably unremove bit
	else	//if(move->flags == MoveRecord::UNREMOVE)
		packet[12] = 0x02;
	packet[13] = 0x00;		//is this list - 1?
	//packet[13] = deadStonesList.size();
	
	packet[14] = move->x;
	packet[15] = move->y;
	receivedOppDone = false;
	
	/*for(i = 0; i < deadStonesList.size(); i++)
	{
		packet[16 + (i * 2)] = deadStonesList[i].x;
		packet[17 + (i * 2)] = deadStonesList[i].y;
	}*/
	
//e2 b3 10 00 ec 05 fa 01 ec 05 01 00 01 00 03 04
//e2 b3 10 00 ec 05 fa 01 ec 05 98 66 01 00 03 10
#ifdef RE_DEBUG
	printf("remove stones packet: ");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, 0x0c);
	qDebug("Sending remove stones");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending remove stones packet");
	delete[] packet;
}

/* Looks like we send this, for instance, on the third pass.
 * FIXME note that a lot of messages have similar headers, we might
 * want something to just write them all */
void TygemConnection::sendEnterScoring(unsigned int game_number)
{
	unsigned int length = 0x30;
	unsigned char * packet = new unsigned char[length];
	int ordinal;
	char qualifier;
	int i;
	char * their_name, * our_name;
	GameData * gd;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us a send enter scoring");
		return;
	}
	gd = boarddispatch->getGameData();
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x69;
	
	packet[4] = (game_number >> 8);
	packet[5] = game_number & 0x00ff;
	packet[6] = (gd->black_name != getUsername()) ^ gd->white_first_flag;
	packet[7] = 0x01;
	
	//their name and rank, our name and our number/color and zeroes
	PlayerListing * opponent = getDefaultRoomDispatch()->getPlayerListing(gd->black_name == getUsername() ? gd->white_name : gd->black_name);
	if(!opponent)
	{
		qDebug("Can't get opponent listing for match request\n");
		return;
	}
	their_name = (char *)opponent->name.toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 8] = their_name[i];
	for(i = strlen(their_name) + 8; i < 22; i++) 	  
		packet[i] = 0x00;
	packet[22] = 0x00;
	sscanf(opponent->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[23] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[23] = ordinal + 0x1a;
	else
		packet[23] = ordinal + 0x11;
	
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 24] = our_name[i];
	for(i = strlen(our_name) + 24; i < 34; i++) 	  
		packet[i] = 0x00;
	packet[34] = 0x00;
	packet[35] = 0x02;		//color? probably not second pass...
	for(i = 0; i < length; i++)
		packet[i] = 0x00;
	
	encode(packet, (length / 4) - 2);
	qDebug("Sending enter scoring");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending enter scoring");
	delete[] packet;
}

void TygemConnection::sendDoneScoring(unsigned int game_code, unsigned short opp_id)
{
	unsigned int length = 14 + (deadStonesList.size() * 2) + (deadStonesList.size() ? 2 : 0);
	unsigned char * packet = new unsigned char[length];
	int i;
	
	if(receivedOppDone)
	{
		packet[0] = 0xe7;
		packet[1] = 0xb3;
	}
	else
	{
		packet[0] = 0xf1;
		packet[1] = 0xb3;
	}
	packet[2] = length & 0x00ff;
	packet[3] = (length >> 8);
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	/* I think bytes 8 and 9 are opponents id if we haven't
	 * received a done from them yet and ours if we have. FIXME */
	if(receivedOppDone)
	{
		packet[8] = our_player_id & 0x00ff;
		packet[9] = (our_player_id >> 8);
	}
	else
	{
		packet[8] = opp_id & 0x00ff;
		packet[9] = (opp_id >> 8);
	}
	//packet[8] = our_player_id & 0x00ff;	//why our id again??
	//packet[9] = (our_player_id >> 8);
	//packet[10] = (move.number & 0x00ff);		//what the hell are these?
	//packet[11] = (move.number >> 8);		// FIXME scores???
	//packet[10] = 0x00;		//possible scores, random?
	if(receivedOppDone)
	{
		packet[10] = done_response & 0x00ff;
		packet[11] = (done_response >> 8);
	}
	else
	{
		qDebug("custom packet baby");
		packet[10] = 0x82;		//these seems to be kept...
		packet[11] = 0x45;		//should be random? = 63?
	}
	
	packet[12] = 0x01;
	packet[13] = deadStonesList.size();
	if(deadStonesList.size())
	{
		/* We're supposed to send the last one */
		packet[14] = deadStonesList[deadStonesList.size() - 1].x;
		packet[15] = deadStonesList[deadStonesList.size() - 1].y;
		for(i = 0; i < (int)deadStonesList.size(); i++)
		{
			packet[16 + (i * 2)] = deadStonesList[i].x;
			packet[17 + (i * 2)] = deadStonesList[i].y;
		}
	}
	
//f1 b3 18 00 ec 05 fa 01 ec 05 98 82 01 04 03 10
//11 0f 11 05 03 04 03 10
#ifdef RE_DEBUG
	printf("done scoring packet: ");
	for(i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, 0x0c);
	qDebug("Sending done scoring");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending done scoring");
	delete[] packet;
	
	if(receivedOppDone)
	{
		sendMatchResult(game_code);
	}
}

/* I don't think we're allowed to send this in score phase or
 * after opponent has pressed done or something */
void TygemConnection::sendResign(unsigned int game_code)
{
	unsigned int length = 0xc;
	unsigned char * packet = new unsigned char[length];
	packet[0] = 0xb0;
	packet[1] = 0xb3;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = game_code & 0x00ff;
	packet[7] = (game_code >> 8);
	packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
//b0 b3 0c 00 ec 05 8b 02 00 00 00 00
	encode(packet, 0x0c);
	qDebug("Sending resign");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending resign");
	delete[] packet;
}

void TygemConnection::sendAdjournRequest(void)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(our_game_being_played);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us an adjourn request");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xb5;
	packet[1] = 0xb3;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = aGameData->game_code & 0x00ff;
	packet[7] = (aGameData->game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
	
	encode(packet, 0x0c);
	qDebug("Sending adjourn request");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending adjourn request");
	delete[] packet;
}

/* The three below should be combined to just change
 * that one byte. FIXME */
void TygemConnection::sendAdjourn(void)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(our_game_being_played);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us an adjourn accept");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xba;
	packet[1] = 0xb3;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = aGameData->game_code & 0x00ff;
	packet[7] = (aGameData->game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
	
	encode(packet, 0x0c);
	qDebug("Sending adjourn accept");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending adjourn accept");
	delete[] packet;
}

void TygemConnection::sendRefuseAdjourn(void)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	GameData * aGameData;
	BoardDispatch * boarddispatch = getIfBoardDispatch(our_game_being_played);
	if(!boarddispatch)
	{
		qDebug("How can there not be a board dispatch, it just sent us an adjourn refusal");
		return;
	}
	aGameData = boarddispatch->getGameData();
	packet[0] = 0xbf;
	packet[1] = 0xb3;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = aGameData->game_code & 0x00ff;
	packet[7] = (aGameData->game_code >> 8);
	if(aGameData->white_name == getUsername())
		packet[8] = 0x01;
	else
		packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;
	
	encode(packet, 0x0c);
	qDebug("Sending adjourn refuse");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending adjourn refusal");
	delete[] packet;
}
	
/* Loser sends this after resign, but I think only if they offered
 * the game */
void TygemConnection::sendMatchResult(unsigned short game_code)
{
	unsigned int length = 0x30;
	unsigned char * packet = new unsigned char[length];
	char * their_name, * our_name;
	int i, ordinal;
	char qualifier;
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for match result send", game_code);
		return;
	}
	/* We need to notify boarddispatch of finished game so we can
	 * get the full result */
	boarddispatch->recvResult(0);
	GameData *aGameData = boarddispatch->getGameData();
	packet[0] = (length >> 8);
	packet[1] = length & 0x00ff;
	packet[2] = 0x06;
	packet[3] = 0x70;
	packet[4] = (game_code >> 8);
	packet[5] = game_code & 0x00ff;
	
	//packet[6] = 0x00;	//winner color
	packet[6] = (aGameData->black_name != getUsername()) ^ aGameData->white_first_flag;
	packet[7] = 0x01;
	
	PlayerListing * opponent = getDefaultRoomDispatch()->getPlayerListing(aGameData->black_name == getUsername() ? aGameData->white_name : aGameData->black_name);
	if(!opponent)
	{
		qDebug("Can't get opponent listing for match request\n");
		return;
	}
	/* FIXME This header is same as sendEnterScoring, combine */
	their_name = (char *)opponent->name.toLatin1().constData();
	for(i = 0; i < strlen(their_name); i++)
		packet[i + 8] = their_name[i];
	for(i = strlen(their_name) + 8; i < 22; i++) 	  
		packet[i] = 0x00;
	packet[22] = 0x00;
	sscanf(opponent->rank.toLatin1().constData(), "%d%c", &ordinal, &qualifier);
	if(qualifier == 'k')
		packet[23] = 0x12 - ordinal;
	else if(qualifier == 'p')
		packet[23] = ordinal + 0x1a;
	else
		packet[23] = ordinal + 0x11;
	//their_name = (char *)opponent->name.toLatin1().constData();
	our_name = (char *)getUsername().toLatin1().constData();
	for(i = 0; i < strlen(our_name); i++)
		packet[i + 24] = our_name[i];
	for(i = strlen(our_name) + 24; i < 34; i++) 	  
		packet[i] = 0x00;
	packet[34] = 0x00;
	packet[35] = 0x02;		//this could actually be type of win, probably color	
	/*
	if(!aGameData->fullresult)
	{
		qDebug("No result on game data to send result");
		return;
	}
	float margin = aGameData->fullresult->winner_score - aGameData->fullresult->loser_score;
	packet[10] = (int)margin & 0x00ff;
	packet[11] = ((int)margin >> 8);
	*/	
	for(i = 36; i < length; i++)
		packet[i] = 0x00;
	
	encode(packet, (length / 4) - 2);
	qDebug("Sending match result");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending match result");
	delete[] packet;
}

/* It seems like this appears like a match invite if opponent closes
 * the window */
void TygemConnection::sendRematchRequest(void)
{
	unsigned int length = 10;
	unsigned char * packet = new unsigned char[length];
	if(!room_were_in)
	{
		qDebug("Trying to send rematch request but not in a room");
		return;
	}
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for rematch send", room_were_in);
		return;
	}
	GameData * gr = boarddispatch->getGameData();
	packet[0] = 0x69;
	packet[1] = 0xc3;
	packet[2] = 0x0a;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	packet[6] = gr->opponent_id & 0x00ff;
	packet[7] = (gr->opponent_id >> 8);
	packet[8] = 0x12;	//?
	packet[9] = 0x00;	//?
	
	encode(packet, 0xa);
	qDebug("Sending rematch request");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending rematch request");
	delete[] packet;
}

void TygemConnection::sendRematchAccept(void)
{
	unsigned int length = 10;
	unsigned char * packet = new unsigned char[length];
	if(!room_were_in)
	{
		qDebug("Trying to send rematch accept but we aren't in room");
		return;
	}
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for rematch accept send", room_were_in);
		return;
	}
	GameData * gr = boarddispatch->getGameData();
	packet[0] = 0x6e;
	packet[1] = 0xc3;
	packet[2] = 0x0a;
	packet[3] = 0x00;
	packet[4] = gr->opponent_id & 0x00ff;
	packet[5] = (gr->opponent_id >> 8);
	packet[6] = our_player_id & 0x00ff;
	packet[7] = (our_player_id >> 8);
	packet[8] = 0x12;	//?	 is this a room?
	packet[9] = 0x00;	//?
	
	encode(packet, 0xa);
	qDebug("Sending rematch request accept");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending rematch request accept");
	delete[] packet;
	playing_game_number = room_were_in;	//so d2af is ready
	//prepare for rematch func?
}

/* I think this double as both our odd/even guess as well
 * as the number we send.  But I think... well I assume
 * that the challenged player is holding the stones out? */
void TygemConnection::sendNigiri(unsigned short game_code, bool odd)
{
	unsigned int length = 12;
	unsigned char * packet = new unsigned char[length];
	unsigned short room_number = game_code_to_number[game_code];
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_number);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for nigiri send", room_number);
		return;
	}
	GameData * gr = boarddispatch->getGameData();
	packet[0] = 0xf5;
	packet[1] = 0xaf;
	packet[2] = 0x0c;
	packet[3] = 0x00;
	packet[4] = game_code & 0x00ff;
	packet[5] = (game_code >> 8);
	packet[6] = room_number & 0x00ff;
	packet[7] = (room_number >> 8);
	//packet[8] = rand() % 255;	//this is both odd and even
	/* If its 0x0_, it looks like its an odd choice
	 * 0x2_ looks like an even choice,
	 * then the second four bits seem to be the number that gets
	 * sent.  But we can still seemingly confuse the oro client,
	 * like we're supposed to send something else... it lets it
	 * play for the right turn, but the colors aren't set.  And
	 * then in addition, we can't seem to play, like we're supposed
	 * to send something else first
	 * But so its like the first 4 bits are our choice of odd 0 or even 2
	 * and then the second 4 bits are the number of stones that our
	 * opponent is holding.  But actually, I've seen 0, 2, and 3 sent!*/
	packet[8] = (rand() % 0xd) + 2;
	packet[8] |= (rand() % 0x19) + 1;
	we_send_nigiri = true;
	//QMessageBox::information(0 , tr("sending niri"), tr("sending ") + QString::number(packet[8]) + tr(" nigiri."));
	
	//if((((packet[8] & 0xe0) >> 4) % 2) == ((packet[8] & 0x1f) % 2))	 
	if(((((packet[8] & 0xe0) >> 5) + 1) % 2) == ((packet[8] & 0x1f) % 2))   
	{
		qDebug("nigiri successful");
		boarddispatch->recvKibitz(QString(), "Opponent plays white");
		if(gr->black_name != getUsername())
			boarddispatch->swapColors();
		else
			boarddispatch->swapColors(true);
	}
	else
	{
		qDebug("nigiri failure");
		boarddispatch->recvKibitz(QString(), "Opponent plays black");
		if(gr->white_name == getUsername())
			boarddispatch->swapColors();
		else
			boarddispatch->swapColors(true);
	}
	
	packet[9] = 0x00;	//?
	packet[10] = 0x00;
	packet[11] = 0x00;
	
	/* Its possible to send this and confuse the ORO client 
	 * actually, if we send this, when we are challenging,
	 * it thinks that the ORO client sent it.. ?!?! */
#ifdef RE_DEBUG
	printf("nigiri packet: ");
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, 0xc);
	qDebug("Sending nigiri");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending nigiri");
	delete[] packet;
	
	gr = boarddispatch->getGameData();	//get again
	startMatchTimers(gr->black_name == getUsername());
	
	// we seem to immediately send keep alive here
	if(gr->black_name == getUsername())
		sendGameUpdate(gr->game_code);
}


#ifdef FIXME
/* FIXME, let's get this one working first in a joined room game */
void TygemConnection::sendGameChat(const GameData & game, unsigned char * text)
{
	unsigned int length = 8 + strlen(text);
	unsigned char * packet = new unsigned char[length];
	packet[0] = 0xec;
	packet[1] = 0x59;
	packet[2] = length & 0x00ff;
	packet[3] = (length >> 8);
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	
	packet[6] = 0x00;
	packet[7] = 0xec;	//special byte?? I have no idea could be first byte
				// of player id FIXME
	for(i = 8; i < (int)length; i++)
		packet[i] = text[i - 8];
	
//ec 59 1c 00 d3 04 00 ec text
	encode(packet, 0x08);
	qDebug("Sending game chat");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending game chat");
	delete[] packet;
}

#endif //FIXME
/* As in leave a played or in room game?  Maybe rooms as well? 
 * Double check, all those zeroes are suspicious. 
 * Actually, probably same as sendObserve message for joining
 * a game? room?  but with no possible password and no id.
 * Yeah, leave is basically the same thing as joining a room
 * except it joins the 00 room,  I guess the lobby. 
 * Also seems like we can observe after joining other rooms
 * without error, it just moves us. */
void TygemConnection::sendLeave(const GameListing & game)
{
	/* Find why leaving a joined game sends two leaves... */
	if(room_were_in == 0)
	{
		qDebug("Attempted to sendLeave but in lobby!");
		return;
	}
	unsigned int length = 0x12;
	unsigned char * packet = new unsigned char[length];
	int i;
	packet[0] = 0xde;
	packet[1] = 0x5d;
	packet[2] = 0x12;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	for(i = 6; i < (int)length; i++)
		packet[i] = 0x00;
//de 5d 12 00 ec 05 00 00 00 ...
	encode(packet, 0x12);
	qDebug("Sending leave");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending leave");
	delete[] packet;
	setRoomNumber(0);
}

/*void TygemConnection::sendLeaveGame(const GameListing & game)
{
	if(room_were_in == 0)
	{
		qDebug("Attempted to sendLeave but in lobby!");
		return;
	}
	unsigned int length = 14;
	unsigned char * packet = new unsigned char[length];
	int i;
	packet[0] = 0x45;
	packet[1] = 0x9c;
	packet[2] = 0x0e;
	packet[3] = 0x00;
	packet[4] = our_player_id & 0x00ff;
	packet[5] = (our_player_id >> 8);
	for(i = 6; i < (int)length; i++)
		packet[i] = 0x00;
//de 5d 12 00 ec 05 00 00 00 ...
	encode(packet, 0xe);
	qDebug("Sending leave game");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending leave");
	delete[] packet;
	setRoomNumber(0);
}*/

/* This really needs to be a whole packet of stuff but for now,
 * since the full thing reqiures some chinese translation: */
void TygemConnection::sendInvitationSettings(bool invite)
{
	unsigned int length = 0x0c;
	unsigned char * packet = new unsigned char[length];
	
	packet[0] = 0x00;
	packet[1] = 0x0c;
	packet[2] = 0x06;
	packet[3] = 0xa2;
	packet[4] = (invite ? 0 : 2);
	packet[5] = 0xf0;
	packet[6] = 0xad;
	packet[7] = 0xba;
	packet[8] = 0x00;
	packet[9] = 0x00;
	packet[10] = 0x00;
	packet[11] = 0x00;	//weird byte???

#ifdef RE_DEBUG
	printf("Sending invite settings: ");
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
#endif //RE_DEBUG
	encode(packet, (length / 4) - 2);
	printf("encoded: ");
	for(int i = 0; i < (int)length; i++)
		printf("%02x ", packet[i]);
	printf("\n");
	qDebug("Sending invitation settings\n");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending invitation settings");
	delete[] packet;
}

void TygemConnection::sendDisconnectMsg(void)
{	
	unsigned int length = 8;
	unsigned char * packet = new unsigned char[length];
	
	packet[0] = 0x00;
	packet[1] = 0x08;
	packet[2] = 0x06;
	packet[3] = 0x51;
	packet[4] = 0x00;
	packet[5] = 0x00;
	packet[6] = 0x00;
	packet[7] = 0x00;
	
	qDebug("Sending server disconnect");
	if(write((const char *)packet, length) < 0)
		qWarning("*** failed sending server disconnect");
	delete[] packet;
}

void TygemConnection::encode(unsigned char * p, unsigned int cycles)
{
	int i;
	unsigned long a, b, c = 0;
	unsigned long header = *(unsigned long *)p;
	header ^= encode_offset;
	p += 4;
	while(cycles--)
	{
		b = *(unsigned long *)p;
		c ^= b;
		b ^= header;
		*(unsigned long *)p = b;
		p += 4;
	}
	*(unsigned long *)p = c;
}

void TygemConnection::handlePassword(QString msg)
{
	qDebug(":%d %s\n", msg.size(), msg.toLatin1().constData());
	if(msg.contains("Password:") > 0 || msg.contains("1 1") > 0)
	{
		qDebug("Password or 1 1 found\n");
		sendText(password.toLatin1().constData());	
		//authState = SESSION;
	}
}

bool TygemConnection::isReady(void)
{
	if(connectionState == CONNECTED)
		return 1;
	else
		return 0;
}

/* Non IGS servers can override this function and pass
 * their own GSName type for now */
void TygemConnection::handleMessage(QString)
{
}

/* We may convert everything here to unsigned char *s, delete the old...
 * we're still sort of hung up on all the things we were doing
 * to work with the IGS code. */
void TygemConnection::handleMessage(unsigned char * msg, unsigned int size)
{
	unsigned short message_type = msg[3] + (msg[2] << 8);
	int i;
	BoardDispatch * bd;
	//message_type = msg[0];
	//message_type >>= 8;
	//message_type += msg[1];
	/* Third byte is likely the size of the message meaning multiple
	 * messages per packet, etc. */
	/*if((message_type & 0x00ff) == 0x00b3)
	{*/
		/*printf("****: \n");
		for(i = 0; i < size; i++)
			printf("%02x", msg[i]);
		printf("\n");*/
	/*}*/
	
	msg += 4;
	size -=4;
	switch(message_type)
	{
		case TYGEM_GAMESINIT:
		case TYGEM_GAMESUPDATE:
			handleGamesList(msg, size);
			break;
	// possibly update after game won, first 00df
	//009806130002000000df0000000100020000ff00706c6d64000000000000000000000011706c6d64003200390000000294ca
	//8ee1944c000000000000000000116c6c36366c6c006e673200010004000000110000010000020000ff00d0c4ccacc8e7c7ef
	//31000000000000006c6f6f706f706f7000390002ccecc9bdc0cf4b0000000000000000086c616f6b31320068657200020001
	//0000

			
		case TYGEM_PLAYERSINIT:
		case TYGEM_PLAYERSUPDATE:
			handlePlayerList(msg, size);
			break;
		case 0x0617:
			handleServerRoomChat(msg, size);
			break;
		case 0x0618:
			printf("0x0618: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0637:
			handleCreateRoomResponse(msg, size);
			break;
		case 0x0638:
			printf("0x0638: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			//break;
		case 0x0639:	//in response to 0638
			handleBoardOpened(msg, size);
			break;
		case 0x0643:
			//0x0643: 70657465726975730000000000000001706574657269757300000000696e74727573696f6e00000000000
			//		009696e74727573696f6e0000020100ffff
			handleMatchInvite(msg, size);
			printf("0x0643: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0644:
			handleMatchInviteResponse(msg, size);
			break;
		case 0x0645:
			if(msg[43] == 0x02)
				handleMatchOffer(msg, size, offer);
			else if(msg[43] == 0x03)
				handleMatchOffer(msg, size, acknowledge);
			else
				printf("*** 0645 match offer has strange type: %02x!!!\n", msg[43]);
			break;
		case 0x0646:
			handleMatchOffer(msg, size, accept);
			//match offer accept
			//00580646
			//0x0646: 70657465726975730000000000000008696e74727573696f6e000000000000090032012c2801010001
			//000001ffffffff000000000001000000000000706574657269757300000002696e74727573696f6e000002
			//00a00616

			
			//rematch related?
			//0646
			//c4ba d3ea b3bf d4c6 0000 0000 0000 0014
			//6269 7473 0000 0000 0000 0000 0000 0014
			//0003 012c 1e03 0000 0000 001e ffff ffff
			//ffff ffff 0001 0001 0000 0100 6a65 7070
			//3100 0000 0000 0002 6269 7473 0000 0000
			//0000 0002
			printf("0x0646: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x064d:
			printf("0x064d: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0661:	//in game chat
			handleGameChat(msg, size);
			break;
		//0633: 
		//063370657465726975730000000000000008696e74727573696f6e00000000000009706574657269757300000002696e7
		//4727573696f6e000002000000000000020000000000
		
		case TYGEM_OBSERVERSINIT:
		case TYGEM_OBSERVERSUPDATE:
				//maybe we do have to request with oro...
			handleObserverList(msg, size);
			break;
		case TYGEM_MOVE:
			handleMove(msg, size);
			break;
		case 0x0669:	//first pass
			handlePass(msg, size, 1);
			printf("**** 0x066b: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x066a:	//think these are counting messages
			
			break;
		case 0x066b:	//second pass
			//this means enter score I think or third pass, something
			//00100101706574657269757300000000000000086574657269757300000000020000000000000000
			//handlePass(msg, size, 2);
			//00870001baacd0a6bba80000000000000000001168616e7869616f68756100020100000000000000
			printf("**** 0x066b: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			bd = getIfBoardDispatch((msg[0] << 8) + msg[1]);
			if(bd)
			{
				bd->recvEnterScoreMode();
				bd->recvKibitz(QString(), "066b");
			}
			break;
		case 0x0670:	//could be rematch no
			//this is sent as a game result I think,
			//but I wonder what relation to 0672 there
			// is
			//white resigns, then 0670 and then 0672
			//0670 
			//0003 0001 6269 7473 0000 0000 0000 0000
			//0000 0014 6269 7473 0000 0000 0000 0002
			//0000 0000 0000 0000
			bd = getIfBoardDispatch((msg[0] << 8) + msg[1]);
			if(bd)
				bd->recvKibitz(0, "0x670");
			printf("0x0670: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0671:
			handleMatchOpened(msg, size);
			break;	
		case 0x0672:
			handleGameResult(msg, size);
			break;
		case 0x0674:
			//067401f90001
			printf("0x0674: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
			//an REM -1 -1 likely proceeds these as meaning done?
			//this is intrusion to peterius after marking all of his black stones dead
		case 0x067b:   
			handleScoreMsg2(msg, size);
			break;
		case TYGEM_TIME:
			handleTime(msg, size);
			break;
		case 0x0681:
			//068101f7020103050340033b028001000000
			printf("0x0681: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0683:	//clock stop? //enter score?
			handleScoreMsg1(msg, size);
			break;
		case 0x0654:	//0653 requests maybe?
			handleList(msg, size);
			//0100000c79bd091300000b9b79bd092d0000023c79bd0914000009e879bd09120000002279bd094b000008c679bd092c0000065b79bd0
			//97d000004c179bd092f0000036a79bd094c000009c379bd09150000000179bd091100000a8c79bd092e00000050
			//00c40616
			break;
		case 0x06c7:
			//this is related to either a failed match create or a bad
			//sent message or maybe something legit
			//00280000696e74727573696f6e00000000000009 396b 0000000000000
			printf("***0x06c7: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		//Two bytestrange: 0692: 
		//0692696e74727573696f6e00000000000009696e74727573696f6e0049020000000000000000000000000000000000000
		//0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
		//0000000000000000000000000000002710006f756e742c2073746f74616c5f636f756e742c20737769000000000000000
		//10000000300000004000000056368696e61005f636f756e742c206c746f74616c5f636f0000000000004218b34d6f7573
		//652e676966006f73735f636f756e742c206c7400000000000000000000000000000000000000000000000000000000004
		//8f80619000000000000000000000000000000000000000000000000
		
		//when we're trying to offer
		//706574657269757300000000000000087065746572697573002f490200000000000000000000000000
		//000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
		//0000000000000000000000000000000000000000000000002328006f756e742c2073746f74616c5f636f756e74
		//2c2073776900000000000000010000000300000004000000056368696e61005f636f756e742c206c746f74616c
		//5f636f000000000000406c8b4d6f7573652e676966006f73735f636f756e742c206c7400000000000000000000
		//0000000000000000000000000000000000000048fa788200000000000000000000000000000000000000000000
		//0000

		
		case 0x0692:
			//possibly related to match opening or negotiation
			printf("0x0692: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0696:
			//some kind of code  0695 requests
			printf("0x0696: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x0698:
			//maybe number of games/players?
			printf("0x0698: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x069f:
			handlePersonalChat(msg, size);
			break;
		case 0x06b1:	//a name during game?  //maybe bets?
			printf("0x06b1: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			break;
		case 0x06af:	//login request response?
			printf("0x06af: ");
			for(i = 0; i < (int)size; i++)
				printf("%02x", msg[i]);
			printf("\n");
			sendInvitationSettings(false);
			break;
		default:
#ifdef RE_DEBUG
			msg -= 4;	//FIXME
			printf("Two bytestrange: %02x%02x: \n", msg[2], msg[3]);
//#ifdef RE_DEBUG
			for(i = 2; i < (int)size + 4; i++)
				printf("%02x", msg[i]);
			printf("\n");
#endif //RE_DEBUG
			break;
	}
}

void TygemConnection::handleConnected(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	if(size != 10)
		qDebug("handleConnected size: %d", size);
	our_player_id = p[0] + (p[1] << 8);
	qDebug("our id: %02x%02x %02x\n", p[0], p[1], p[2]);
	our_special_byte = p[2];
	p += 6;
	
	//try this here
	//sendInvitationSettings(true);	//for now
	// FIXME, toggle is doing this now, but is it kosher with initial state?
	// i.e., when we first join what happens? where does that get checked?
}

//#define PLAYERLIST_DEBUG
//0613
void TygemConnection::handlePlayerList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned char name[11];
	QString encoded_name, ascii_name, rank;
	int players;
	unsigned char rankbyte, invitebyte;
	int i;
	unsigned short id;
	bool no_rank = false;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * newPlayer = new PlayerListing();
	PlayerListing * aPlayer;
	newPlayer->online = true;
	newPlayer->info = "??";
	newPlayer->playing = 0;
	newPlayer->observing = 0;
	newPlayer->idletime = "-";
	p += 4;

	//make sure these aren't one off, FIXME,
	//i.e., cutting off last record, also check on ORO
	while(p < (msg + size - 0x37) || (p < (msg + size - 19) && p[0] == 0x01))
	{
		//0002 009e 616c 6578 3238 3200 0000 0000 
		//0000 0015 616c 6578 3238 3200 6100 0002 
		//0000 0000 0000 00fd 0000 00de 0000 0006 
		//0000 552f 0000 004b
		
		//longp7 103 84 28166 bu ke
		//0002 009e 6c6f 6e67 7037 0000 0000 0000
		//0000 0017 6c6f 6e67 7037 0069 6c00 0002
		//0000 0000 0000 0067 0000 0054 0000 0003
		//0000 6e06 0101 004b
		
		
		//0100 0000 c1c9 c0ab befd 0000 0000 0000
		//0000 0015 
		//0002 009e d4c6 d6ae d7d3 a3b0
		//a3b1 0000 0000 0013 796d 797a 7a006e616f7a000200000000
#ifdef PLAYERLIST_DEBUG
		for(int j = 0; j < 0x38; j++)
			printf("%c", p[j]);
		printf("\n");
		for(int j = 0; j < 0x38; j++)
			printf("%02x", p[j]);
		printf("\n");
#endif //PLAYERLIST_DEBUG
		if(p[0] == 0x01)
		{
			p += 4;
			strncpy((char *)name, (char *)p, 11);
			encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
			aPlayer = roomdispatch->getPlayerListing(encoded_name);
			if(aPlayer)
			{
				aPlayer->online = false;
				roomdispatch->recvPlayerListing(aPlayer);
			}
			else
				qDebug("Can't find disconnecting player");
			p += 14;
//#ifdef PLAYER_LIST_DEBUG
			printf("Player %s disconnected, last bytes: %02x%02x\n", encoded_name.toLatin1().constData(), p[0], p[1]);
//#endif //PLAYERLIST_DEBUG
			p += 2;
			continue;
		}
		p += 2;
		p++;
		if(!p[0])
		{
			//often [Gxxxx] player, I assume guest?
			//0002 0000 5b47 3236 3734 5d32 3000 0000
			//0000 0000 5b47 3236 3734 5d32 3000 0000
			//0000 0100 0000 0000 0000 0000 0000 0000
			//0000 45ab 0001 004b
#ifdef PLAYERLIST_DEBUG
			printf("Guest\n");
#endif //PLAYERLIST_DEBUG
			p += 0x35;
			continue;
		}
		/* I think p[0] is the flag icon, but it can be different
		 * pictures too so... */
		p++;
		strncpy((char *)name, (char *)p, 11);
		encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
		p += 14;
		p++;
		//rank byte
		if(p[0] < 0x12)
			rank = QString::number(0x12 - p[0]) + 'k';
		else if(p[0] > 0x1a)
			rank = QString::number(p[0] - 0x1a) + 'p';
		else
			rank = QString::number(p[0] - 0x11) + 'd';
		p++;
		strncpy((char *)name, (char *)p, 11);
		ascii_name = QString((char *)name);
		//another name
		aPlayer = roomdispatch->getPlayerListing(encoded_name);
		if(!aPlayer)
			aPlayer = newPlayer;
		aPlayer->name = encoded_name;
		aPlayer->ascii_name = ascii_name;
		aPlayer->rank = rank;
		p += 8;
		p += 8;
		// are these unsigned longs or shorts ?!?!?
		p += 2;
		aPlayer->wins = (p[0] << 8) + p[1];
		p += 2;
		p += 2;
		aPlayer->losses = (p[0] << 8) + p[1];
		p += 2;	//w/l
		p += 4;
		if(p[0] == 0xff && p[1] == 0xff)   //normally 0000 ----, then ffff ff40
			no_rank = true;
		p += 2;
		//0002 009e 3737 3435 3532 0000 0000 0000 
		//0000 0000 3737 3435 3532 006f 6f6e 0002
		//0000 0000 0000 0064 0000 012b 0000 0000
		//ffff ff40 0001 004b
		if(no_rank)
		{
			aPlayer->rank_score = 0;
			no_rank = false;
		}
		else
			aPlayer->rank_score = (p[0] << 8) + p[1];
		p += 2;
		//p[0] probably means keyi, can contact with game
		if(p[0])
			aPlayer->info = "X";
		else
			aPlayer->info = "O";
		//p[1]
		//00  
		//01  might mean observing
		//02  probably means in a game
		aPlayer->country = QString::number(p[0], 16) + QString::number(p[1], 16);
		p += 2;
		p += 2;
#ifdef PLAYERLIST_DEBUG
		printf("%s %s", name, rank.toLatin1().constData());
#endif //PLAYERLIST_DEBUG
		roomdispatch->recvPlayerListing(aPlayer);
	}
	delete newPlayer;
	
	if(p != (msg + size))
	{
		qDebug("handlePlayerListing packet strange size %d", (msg + size) - p);
	}
	return;
	
	PlayerListing * backPlayer;
	
	
	players = p[2];

#ifdef RE_DEBUG
	printf("%02x%02x\n", p[0], p[1]);	
#endif //RE_DEBUG	
	p += 2;
	//676c696c696e6e000000 770a 16a4 02c3 0000 9b72 e607 e407 0000 0056
	while(p < (msg + size))
	{
#ifdef RE_DEBUG
		for(i = 0; i < 28; i++)
			printf("%02x", p[i]);
		printf("\n");
#endif //RE_DEBUG
		strncpy((char *)name, (char *)p, 10); name[10] = 0x00;
#ifdef RE_DEBUG
		printf("%s ", name);
#endif //RE_DEBUG
		p += 10;
		id = p[0] + (p[1] << 8);
		aPlayer = roomdispatch->getPlayerListing(id);
		if(!aPlayer)
		{
			aPlayer = newPlayer;
			newPlayer->playing = 0;
		}
		aPlayer->id = id;
		//aPlayer->name = QString((char*)name);
		aPlayer->name = serverCodec->toUnicode((const char *)name, strlen((const char *)name));
		
		p += 2;
		//a byte here seems to be a send msg byte, or something
		aPlayer->specialbyte = p[0];
		//p++;
#ifdef RE_DEBUG
		for(i = 0; i < 8; i++)
			printf("%02x", p[i]);
#endif //RE_DEBUG
		// That's right, its the pro bit 
		rankbyte = p[1];
		if(rankbyte & 0x40)
		{
			aPlayer->pro = true;
		}
		
		//lost interest, its not the game
		//aPlayer->observing = p[2];	//FIXME, what is this?
		//this fourth byte here is country I think.
		// but its really unreliable, almost like its something
		// that clients from these countries have as a setting
		// or maybe part of a bit field.
		aPlayer->country = getCountryFromCode(p[3]);
		//fifth byte is their open status
		invitebyte = p[4];
		aPlayer->nmatch_settings = QString::number(p[0]) + QString(" ") + QString::number((p[1])) + QString(" ") + QString::number(p[2]) + QString(" ") + QString::number(p[3]);
		p += 6;
		aPlayer->rank_score = p[0] + (p[1] << 8); 
		if(aPlayer->pro)
			aPlayer->rank = QString::number(rankbyte & 0x1f) + QString("dp");
		else
			aPlayer->rank = rating_pointsToRank(aPlayer->rank_score);
		aPlayer->info = getStatusFromCode(invitebyte, aPlayer->rank);
		aPlayer->wins = p[2] + (p[3] << 8);
		aPlayer->losses = p[4] + (p[5] << 8);
		p += 6;
#ifdef RE_DEBUG
		printf(" RP: %d W/L: %d/%d ", aPlayer->rank_score, aPlayer->wins, aPlayer->losses);
		for(i = 0; i < 4; i++)
			printf("%02x", p[i]);
		printf("\n");
#endif //RE_DEBUG
		p += 3;
		p++;
		roomdispatch->recvPlayerListing(aPlayer);
		/* Its ugly-slow to do this here but I don't know what
		 * other choice we have */
		if(rooms_without_owners.size())
		{
			std::vector<GameListing *>::iterator it;
			for(it = rooms_without_owners.begin(); it != rooms_without_owners.end(); it++)
			{
				if((*it)->owner_id == aPlayer->id)
				{
#ifdef RE_DEBUG
					qDebug("Found room %d with owner %s\n", (*it)->number, aPlayer->name.toLatin1().constData());
#endif //RE_DEBUG
					backPlayer = roomdispatch->getPlayerListing(aPlayer->id);
					if(!backPlayer)
					{
						qDebug("But couldn't find owner");
						break;
					}
					(*it)->white = backPlayer;
					setAttachedGame(backPlayer, (*it)->number);
					rooms_without_owners.erase(it);
					break;
				}
			}
		}
		
		aPlayer->pro = false;
	}
	//FIXME test without this
	delete newPlayer;
#ifdef RE_DEBUG
	printf("*** Players %d\n", players);
#endif //RE_DEBUG
}

QString TygemConnection::rating_pointsToRank(unsigned int rp)
{
	int rem = rp % 1000;
	rp -= rem;
	rp /= 1000;
	if(rp < 26)
	{
		rp = 26 - rp;
		return QString::number(rp) + QString("k");
	}
	else
	{
		rp -= 25;
		return QString::number(rp) + QString("d");
	}
	return QString("?");
}

unsigned int TygemConnection::rankToScore(QString rank)
{
	QString buffer = rank;
	//buffer.replace(QRegExp("[pdk+?\\*\\s]"), "");
	buffer.replace(QRegExp("[pdk]"), "");
	int ordinal = buffer.toInt();
	unsigned int rp;

	if(rank.contains("k"))
		rp = (26 - ordinal) * 1000;
	else if(rank.contains("d"))
		rp = 25000 + (ordinal * 1000);
	else if(rank.contains("p"))
		rp = 36000 + (ordinal * 1000);
	else
		return 0;
	
	return rp;
}

/* FIXME What are the other bits for ?!?!?!*/
QString TygemConnection::getCountryFromCode(unsigned char code)
{
	code &= 0x0f;
	switch(code)
	{
		case 1:
			return "KOREA";
			break;
		case 2:
			return "CHINA";
			break;
		case 3:
			return "JAPAN";
			break;
		case 4:
			return "INTL";
			break;
		case 5:
			return "THAILAND";
			break;
		case 6:
			//that blue flag...
			return "UN?";
			break;
		case 7:	
			return "USA";
			break;
		case 8:
			return "HONGKONG";
			break;
		case 9:
			return "SINGAPORE";
			break;
		case 0xa:
			return "CANADA";
			break;
		case 0xb:
			return "GERMANY";
			break;
		case 0xc:
			return "FRANCE";
			break;
		case 0xd:
			return "AUSTRAILIA";
			break;
		case 0xe:
			return "RUSSIA";
			break;
		case 0xf:
			return "UKRAINE";
			break;
	}
	return QString("-");
}

/* These are the number of players on the other servers !!! 
 * FIXME */
//0x0654: 
void TygemConnection::handleList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	int records;
	p += 2;
	records = (p[0] << 8) + p[1];
	p += 2;
#ifdef RE_DEBUG
	//printf("0654 %d unknown records: \n", records);
#endif //RE_DEBUG
	while(p < (msg + size - 3))
	{
#ifdef RE_DEBUG
	//	printf("%02x%02x for %d(%02x%02x) %d\n",
	//	       p[0], p[1], (p[2] << 8) + p[3], p[2], p[3],
	//	       (p[6] << 8) + p[7]);
#endif //RE_DEBUG
		p += 8;
	}
		//0100 000d 
		//79bd 0913 0000 0b3f 
		//79bd 092d 0000 0092 
		//79bd 0914 0000 0909 
		//79bd 0912 0000 001a 
		//79bd 094b 0000 04b5 
		//79bd 092c 0000 01ee 
		//79bd 097d 0000 00c9 
		//79bd 092f 0000 031a 
		//79bd 094c 0000 0302 
		//79bd 0915 0000 0000 
		//79bd 0911 0000 0729 
		//79bd 092e 0000 005c 
		//79bd 0915 0000 0004
}

//c661
// this is looking like a room list
void TygemConnection::handleRoomList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned int number_of_games;
	unsigned short number;
	int i;
	PlayerListing * player;
	GameListing * aGameListing;
	GameListing * newGameListing = new GameListing();
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();

	number_of_games = p[0] + (p[1] << 8);
	p += 4;
#ifdef RE_DEBUG
	printf("c661 Number of games in this list: %d\n", number_of_games);
#endif //RE_DEBUG
	// 4, 4 for one player, 4 for the second player, another 4
	while(number_of_games--)
	{
		number = p[0] + (p[1] << 8);
		if(number == 0)
		{
			/* There's no 0th record except maybe total number of
			 * games and those 01 tagged records are weird, like
			 * their either empty or they're for particular
			 * players... they might be player stats but they
			 * don't show up in official ORO app so who knows.*/
			/* FIXME, these are probably important and I just
			 * have no idea. */
			p += 16;
			continue;
		}
		aGameListing = roomdispatch->getGameListing(number);
		if(!aGameListing)
		{
			aGameListing = newGameListing;
			newGameListing->black = 0;
			newGameListing->white = 0;
		}
		aGameListing->number = number;	
		aGameListing->running = true;
		unsigned char * q = p;
		for(i = 0; i < 4; i++)
		{
#ifdef RE_DEBUG
			printf("%02x%02x%02x%02x ", q[0], q[1], q[2], q[3]);
#endif //RE_DEBUG
			q += 4;
		}
#ifdef RE_DEBUG
		printf("\n");
#endif //RE_DEBUG
		if(p[4] & 0x70)
			aGameListing->isLocked = true;
		//p[4] & 0x70 = locked game password 
		/* I think phase is or can be unreliable for broadcast
		 * games, maybe others */
		/*FIXME, setting observers here skews setAttached*/
		aGameListing->observers = p[2];
		aGameListing->moves = getPhase(p[6]);
		if(aGameListing->moves == -1)
			aGameListing->moves = 0;
		aGameListing->owner_id = p[8] + (p[9] << 8);
		player = roomdispatch->getPlayerListing(aGameListing->owner_id);
		if(player)
			aGameListing->white = player;
		
		/* These are really like room owners */
		if(!aGameListing->black)
		{
			//new sets this to 0
			aGameListing->_black_name = getRoomTag(p[4]);
			aGameListing->isRoomOnly = true;
			//aGameListing->observers--;	//one attached
		}
		roomdispatch->recvGameListing(aGameListing);
		if(player)
		{
			//aGameListing->observers--;
			//after recv
			setAttachedGame(player, aGameListing->number);
		}	
		p += 16;
		if(!player)
		{
			aGameListing = roomdispatch->getGameListing(number);
			rooms_without_owners.push_back(aGameListing);
		}
#ifdef RE_DEBUG
		printf("Observers: %d\n", aGameListing->observers);
#endif //RE_DEBUG
	}
	if(p != msg + size)
		qDebug("handleRoomList: doesn't match size %d", size);
	//FIXME try without
	delete newGameListing;
}

/* We might FIXME replace the return on this with a QString
 * when we alter the list view to handle different columns.
 * But for now, moves is pretty accurate for the field */
int TygemConnection::getPhase(unsigned char byte)
{
	switch(byte)
	{
		case 0:
			return 0;
					//open
			break;
		case 1:	
			return 30;
					//early
			break;
		case 2:
			return 50;
					//middle
			break;
		case 3:
			return 130;
					//end
			break;
		case 11:
					//looks like wished opponent
		default:
			return -1;
			break;
	}
}

QString TygemConnection::getRoomTag(unsigned char byte)
{
	switch(byte)
	{
		case 0x1f:
		case 0x3d:
					//normal and broadcast in progress
			return QString();
			break;
		case 0x04:
			return QString("** chat 04");
			break;
		case 0x1a:
			return QString("** chat 1a");
			break;
		case 0x70:
			//likely chat 0 locked?
			return QString("** chat 70");
			break;
		case 0x3c:
			return QString("** review 3c");
			break;
		case 0x19:
			return QString("** chat 19");
			break;
		case 0x1c:
			return QString("** quit? 19");
			break;
		case 0x0b:
			return QString("** wished 0b");
			break;
		case 0x0f:
			return QString("** one color 0f");
			break;
		default:
			return (QString("** ") + QString::number(byte, 16));
			break;		
	}
}

//f861  These are broadcasted games I think.  They're not players on the service
void TygemConnection::handleBroadcastGamesList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned int number_of_games;
	int number;
	unsigned char name[11];
	name[10] = 0x00;
	int i;
	
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	GameListing * aGameListing;
	GameListing * ag = new GameListing();
	ag->running = true;
	
	number_of_games = p[0] + (p[1] << 8);
	p += 2;
#ifdef RE_DEBUG
	printf("Number of games: %d\n", number_of_games);	
#endif //RE_DEBUG
	while(number_of_games--)
	{
		number = p[0] + (p[1] << 8);
		aGameListing = roomdispatch->getGameListing(number);
		if(!aGameListing)
			aGameListing = ag;
		aGameListing->number = number;
		p += 2;
		aGameListing->game_code = p[0] + (p[1] << 8);
		p += 2;
		strncpy((char *)name, (char *)p, 10);
		aGameListing->black = 0;
		//aGameListing->_black_name = QString((char *)name);
		aGameListing->_black_name = serverCodec->toUnicode((const char *)name, strlen((const char *)name));
		p += 10;
		strncpy((char *)name, (char *)p, 10);
		aGameListing->white = 0;
		//aGameListing->_white_name = QString((char *)name);
		aGameListing->_white_name = serverCodec->toUnicode((const char *)name, strlen((const char *)name));
		
		p += 10;
#ifdef RE_DEBUG
		printf("br %02x wr %02x\n", p[0], p[1]);
#endif //RE_DEBUG
		if((p[0] & 0xC0) == 0xC0)
		{
			aGameListing->_black_rank = QString::number(p[0] & 0x0f) + "p";
			aGameListing->_black_rank_score = 34000 + ((p[0] & 0x0f) * 1000);
		}
		else if((p[0] & 0xA0) == 0xA0)
		{
			aGameListing->_black_rank = QString::number(p[0] & 0x0f) + "d";
			aGameListing->_black_rank_score = 25000 + ((p[0] & 0x0f) * 1000);
		}
		if((p[1] & 0xC0) == 0xC0)
		{
			aGameListing->_white_rank = QString::number(p[1] & 0x0f) + "p";
			aGameListing->_white_rank_score = 34000 + ((p[1] & 0x0f) * 1000);
		}
		else if((p[1] & 0xA0) == 0xA0)
		{
			aGameListing->_white_rank = QString::number(p[1] & 0x0f) + "d";
			aGameListing->_white_rank_score = 25000 + ((p[1] & 0x0f) * 1000);
		}
		p += 2;
		/* The next two bytes are the country codes for each player,
		 * but I'm not sure where to display them. FIXME */
		/* [K] [C] [J], let's not kid ourself about the pro game countries */
		QString country = getCountryFromCode(p[0]);
		aGameListing->_black_name += ("[" + country[0] + "]");
		country = getCountryFromCode(p[1]);
		aGameListing->_white_name += ("[" + country[0] + "]"); 
		p += 2;
		
#ifdef RE_DEBUG
		printf("broadcast %d: ", aGameListing->number);
		for(i = 0; i < 48; i++)
			printf("%02x", (p - 28)[i]);
		printf("\n"); 
#endif //RE_DEBUG
		aGameListing->game_code = aGameListing->number;	//just for these broadcasts
		
		/* Looks like if the last two 4 bytes are 0xf190 ed00, then
		 * its game-over */
		p += 28;
		/* This seems to be wrong */
		if(p[0] == 0xf1 && p[1] == 0x90 && p[2] == 0xed && p[3] == 0x00)
		{
			aGameListing->FR = "Ended";
		}
		p += 4;
		aGameListing->isRoomOnly = false;
		aGameListing->isBroadcast = true;
		aGameListing->observers = 0;	
		//to prevent broadcast games from closing FIXME
		roomdispatch->recvGameListing(aGameListing);
		clearRoomsWithoutGames(aGameListing->number);
	}
	//FIXME try without
	//delete ag;
	if(p != (msg + size))
		qDebug("bad p: line: %d", __LINE__);
}

#define GAMELIST_DEBUG
//0612
void TygemConnection::handleGamesList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned int number_of_games;
	unsigned int id;
	unsigned int name_length;
	unsigned char name[11];
	unsigned char flags;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	int i;
	bool white_first;
	QString nameA, nameB, rankA, rankB;
	QString encoded_nameA, encoded_nameB;
	GameListing * aGameListing;
	GameListing * ag = new GameListing();
	
	ag->running = true;
	
	p += 4;
	while(p < (msg + size - 0x48) || (p < (msg + size - 3) && p[2] == 0x01))
	{
		id = (p[0] << 8) + p[1];
		//0000 0001 0002 0000 ff00 7978 7973 7a00 
		//0000 0000 0000 0000 0010 7978 7973 7a00
		//4441 4f00 0002 6671 7964 5f32 3100 0000
		//0000 0000 0010 6671 7964 5f32 3100 4a00
		//0002 0002 0000
		
		//0000 0001 0000 0000 ff00 0061 7a79 3200
		//0000 0000 0000 0000 0014 7161 7a79 3200
		//6e67 7a69 0002 6478 646b 6b6b 3137 3737
		//0000 0000 0014 6478 646b 6b6b 3137 3737
		//0002 0004 0000

		//0100 024e 0000 0001 0001 0000 ff00 c0b4c8a5cad6000000000000000000106c6169717573686f75000002d0afdec4b1d2c7b1ccd30000000000106
		//96f696e69636b0000720002
		//***
		//02fd 0613 000c 0000 
		
		//024e 0000 0001 0001 0000 ff00 c0b4 c8a5
		//cad6 0000 0000 0000 0000 0010 6c61 6971
		//7573 686f 7500 0002 d0af dec4 b1d2 c7b1
		//ccd3 0000 0000 0010 696f 696e 6963 6b00
		//0072 0002 0004 0000
		
		//0062 0000 000100020000ff004b474b4c5800000000000000000000024b474b4c5800786961
		//000002b7e7c7efd1f400000000000000000002626f626f313031300031000200010000
		//0198 0000 0001 0002 0000 ff00 bbe5c9aed5abc8cb0000000
		//0000000156a61737300363138006f0002bfd5c5d000000000000000000000001562656172643731003638000000020000
		//00c8 0000 0001 0002 0000 ff00 b4f3 c1a6 d0fdb7e700000000000000156c696c6979616e616e000002cfc0bfcd3132360000000000000000157869616b65313236000000020
		//0040000
		//00b5 0000 0001 0000 0000 ff00 0061 7a79
		//3200 0000 0000 0000 0000 0014 7161 7a79
		//3200 6e67 7a69 0002 6478 646b 6b6b 3137
		//3737 0000 0000 0014 6478 646b 6b6b 3137
		//3737 0002 0004 0000 00b5 0100
		//024e 0000 0001 0001 0000 ff00 c0b4 c8a5 cad6000000000000000000106c6169717573686f750
		//00002d0afdec4b1d2c7b1ccd3000000000010696f696e69636b000072000200040000
		
		//02770000000000030000ff00c4b8bca6b2bbcfc2b5b00000
		//00000015656e676a6973680038000002bbaae5d0d2a3000000000000000000156a797032303038006e00000200020000010f0000000100000000f
		//f000068696d7900000000000000000000156368696d79000065007900026b656e743532303530000000000000156b656e74353230353000000200
		//040000010f01000121000004400001ffffff80cab1d4bd00000000000000000000001e536869597565310000000002b9c5c1a6000000000000000
		//00000002347554c4931000000000000020004001d34303030303030b5da3130bdecb0a2baaccdadc9bdb1adb0ebbef6c8fc00dc00000011000400
		//00ff80c9b3c8cc000000000000000000000018796f6e676b61693200000002dec4b3c7bdd0bba8d7d300000000001879636a687a0039006564000
		//200010000

		
		//***
		// one player game ? over?
		//0000 0001 0001 0000ff0070656e6733333300000000000000001270656e67333333007573000200000000000000000000000000000000007171717
		//13531390052000000000000

		//***
		//locked
		//0001 0101 0002 ffffff003230327164680000000000000000001232303271646800676874000232303371646800000000000000000010323033716
		//468003100d6000200040000
		
		//0104000102010001ffff1220b8f9bdc7bef0b4cf32000000000000126d6f6e6773696c00646b00000000000000000
		//000000000000000000000696d30393335000079000000000000
		
		//00040000000100010000ff00cee5c1d6b2fdb8e4000000000000001177756c696e72616f686500020000
		//000000000000000000000000000000000000000000000000000000000000

		//****
#ifdef GAMELIST_DEBUG
		printf("Id: %d:\n", id);
#endif //GAMELIST_DEBUG
		aGameListing = roomdispatch->getGameListing(id);
		if(!aGameListing)
			aGameListing = ag;
		aGameListing->number = id;
		if(p[2] == 0x01)
		{
			// might mean new game?  seems to follow the record
			// for that id, or might mean game over
			// they come up fast but I'm pretty sure this
			// means the game is ended.
//#ifdef GAMELIST_DEBUG
#ifndef GAMELIST_DEBUG
			printf("Id: %d:\n", id);
#endif // !GAMELIST_DEBUG
			printf("is ended!!!\n");
			for(i = 0; i < 4; i++)
				printf("%c", p[i]);
			printf("\n");
//#endif //GAMELIST_DEBUG
			p += 4;
			aGameListing->running = false;
			roomdispatch->recvGameListing(aGameListing);
			continue;
		}
#ifdef GAMELIST_DEBUG
		for(i = 0; i < 0x48; i++)
		{
			printf("%c", p[i]);	
		}
		printf("\n");
		for(i = 0; i < 0x48; i++)
		{
			printf("%02x", p[i]);	
		}
		printf("\n");
#endif //GAMELIST_DEBUG
		p += 2;
		//0000 0001 0002 0000 ff00 7978 7973 7a00 
		p += 2;
		p++;
		//impacts time
		//there's also 0x30, I think 0x50, etc.. issues here
		aGameListing->white_first_flag = p[0] & 0x01;
		/* 04 means done, 06 is waiting room or looking, 01 and 00
		 * probably mean playing */
		aGameListing->FR = QString::number(p[0], 16);
		p++;
		aGameListing->observers = (p[0] << 8) + p[1];
		p += 2;
		p++;
		//aGameListing->FR = "";
		p += 2;
		flags = p[0];
		//0000 0001 0001 ffff ff00 7479633435320000000000000000001274796334353200003800000200000000000000000000000000000000003337323
		//13535380000000000000003

		
		if(flags & 0x80)
		{
			// Betting
			aGameListing->FR += "B";
		}
		if(flags & 0x20)
		{
			//probably broadcast, maybe as well as betting
			aGameListing->FR += "R";
		}
		//ff00
		p++;
		
		//name 1
		strncpy((char *)name, (char *)p, 11);
		encoded_nameA = serverCodec->toUnicode((char *)name, strlen((char *)name));
		p += 15;
		//rank byte
		if(p[0] < 0x12)
			rankA = QString::number(0x12 - p[0]) + 'k';
		else if(p[0] > 0x1a)
			rankA = QString::number(p[0] - 0x1a) + 'p';
		else
			rankA = QString::number(p[0] - 0x11) + 'd';
		p++;
		strncpy((char *)name, (char *)p, 11);
#ifdef GAMELIST_DEBUG
		printf("%s vs.", name);
#endif //GAMELIST_DEBUG
		nameA = QString((char *)name); 
		p += 12;
		if(p[0] == 0x00)
		{
			p += 16;
			encoded_nameB = QString();
			nameB = QString();
			rankB = QString();
		}
		else
		{
			strncpy((char *)name, (char *)p, 11);
			encoded_nameB = serverCodec->toUnicode((char *)name, strlen((char *)name));
			p += 15;
			if(p[0] < 0x12)
				rankB = QString::number(0x12 - p[0]) + 'k';
			else if(p[0] > 0x1a)
				rankB = QString::number(p[0] - 0x1a) + 'p';
			else
				rankB = QString::number(p[0] - 0x11) + 'd';
			p++;
			strncpy((char *)name, (char *)p, 11);		//chinese
#ifdef GAMELIST_DEBUG
			printf(" %s\n", name);
#endif //GAMELIST_DEBUG
			nameB = QString((char *)name); 
		}
		p += 12;
		p++;
		//this p[0] should be game status
		aGameListing->FR += QString::number(p[0], 16);
		p++;
		p++;
		name_length = p[0];
		p++;
		p += name_length;
		if(aGameListing->white_first_flag)
		{
			aGameListing->_white_name = nameA;
			aGameListing->_white_rank = rankA;
			aGameListing->_black_name = nameB;
			aGameListing->_black_rank = rankB;
		}
		else
		{
			aGameListing->_white_name = nameB;
			aGameListing->_white_rank = rankB;
			aGameListing->_black_name = nameA;
			aGameListing->_black_rank = rankA;
		}
		roomdispatch->recvGameListing(aGameListing);
	}
	delete ag;
}

//0456
void TygemConnection::handlePlayerConnect(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned char name[11];
	name[10] = 0x00;
	unsigned char name2[11];
	name2[10] = 0x00;
	unsigned char rankbyte;
	unsigned short id;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	PlayerListing * newPlayer = new PlayerListing();
	PlayerListing * aPlayer;
	newPlayer->online = true;
	newPlayer->info = "??";
	newPlayer->playing = 0;
	newPlayer->observing = 0;
	newPlayer->idletime = "-";
	
	if(size != 42)
		qDebug("handlePlayerConnect of size: %d\n", size);
	strncpy((char *)name, (char *)p, 10);
#ifdef RE_DEBUG
	printf("0456 %s connected, ", name);
#endif //RE_DEBUG
	
	p += 10;
	strncpy((char *)name2, (char *)p, 10);
#ifdef RE_DEBUG
	printf("or player %s, ", name2);
#endif //RE_DEBUG
	
	
	p += 10;	//name again;
#ifdef RE_DEBUG
	printf("id: %02x%02x\n", p[0], p[1]);
#endif //RE_DEBUG
	id = p[0] + (p[1] << 8);
	aPlayer = roomdispatch->getPlayerListing(id);
	if(!aPlayer)
		aPlayer = newPlayer;
	aPlayer->id = id;
	//aPlayer->name = (char *)name;
	aPlayer->name = serverCodec->toUnicode((const char *)name, strlen((const char *)name));
	/* It actually looks like this catches more unicode foreign names, then the
	* second name.  But I get the feeling that one is the text username or
	* something... not sure */
	/* Yeah, FIXME, the first name appears to allow asian characters, the second is just
	 * username.  We might want an option to turn them on and off */
	//aPlayer->name = (char *)name2;
	//aPlayer->name = serverCodec->toUnicode((const char *)name2, strlen((const char *)name2));
		
	/* I think we want to take the first name if we take the second
	* here, I think the second is allowed to be unicode or something...
	* done FIXME, still not sure of this, we should
	* really just add unicode support !!!*/
	
	
	// are you my special msg byte??? FIXME
	p += 2;
	aPlayer->specialbyte = p[0];
	p++;
	rankbyte = p[0];
	if(rankbyte & 0x40)
		aPlayer->pro = true;
	p++;
	aPlayer->wins = p[0] + (p[1] << 8);
	aPlayer->losses = p[2] + (p[3] << 8);
	p += 4;
	p += 4;
	aPlayer->rank_score = p[0] + (p[1] << 8);
	if(aPlayer->pro)
		aPlayer->rank = QString::number(rankbyte & 0x1f) + QString("dp");
	else
		aPlayer->rank = rating_pointsToRank(aPlayer->rank_score);
#ifdef RE_DEBUG
	printf(" RP: %d W/L: %d/%d ", aPlayer->rank_score, aPlayer->wins, aPlayer->losses);
#endif //RE_DEBUG
	p += 2;
#ifdef RE_DEBUG
	for(int i = 0; i < 8; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	aPlayer->country = getCountryFromCode(p[2]);
	aPlayer->info = getStatusFromCode(p[3], aPlayer->rank);
	aPlayer->nmatch_settings = QString("-") + QString::number(msg[23]) + " " + QString::number(p[0]) + QString(" ") + QString::number((p[1])) + QString(" ") + QString::number(p[2]) + QString(" ") + QString::number(p[3]) + QString::number(p[4]) + QString(" ") + QString::number((p[5])) + QString(" ") + QString::number(p[6]) + QString(" ") + QString::number(p[7]);
	p += 8;	//other data;
	aPlayer->observing = 0;		//necessary?
	//aPlayer->playing = 0;	//necessary?
	roomdispatch->recvPlayerListing(aPlayer);
	//FIXME test without this
	//delete newPlayer;
}

/* This is necessary as a safety precaution.  Maybe if we were sure
 * about all aspects of protocol, but for now, let's prevent the
 * crashes. */
/* Starting to think that we shouldn't modify observers here.
 * entering should be enough.  That and the listing on connect */
 /* CyberORO does have multi 1:N games which means that there's possibly
  * some issues where there might be a player attached to multiple games */
 /* Also, FIXME, there's no real need for an observing and a playing with ORO,
  * we should just use observing to determine what room they're in.  Its confusing
  * otherwise.  Although I guess "playing" can be used to mean their name is in the
  * listing.  All of these places where we close empty rooms need to be brought
  * together FIXME*/
void TygemConnection::setAttachedGame(PlayerListing * const player, unsigned short game_id)
{
	if(player->playing && player->playing != game_id)
	{
		GameListing * g = getDefaultRoomDispatch()->getGameListing(player->playing);
		if(g)
		{
			if(g->white == player)
			{
				g->white = 0;
				if(g->black)
				{
					/* This is so the name is always
					 * in the same column */
					g->white = g->black;
					g->black = 0;
				}
				//g->observers--;	
			}
			else if(g->black == player)
			{
				g->black = 0;
				//g->observers--;
			}
			/*if(!g->observers)
			{
				//broadcast games never go down, so there's FIXME
				//issues here
				qDebug("Closing game %d, no observers", g->number);
				g->running = false;
				getDefaultRoomDispatch()->recvGameListing(g);	
			}*/
		}
	}
	else if(player->playing == game_id)
		return;			//nothing to do
#ifdef RE_DEBUG
	printf("Moving %s %p from %d to %d\n", player->name.toLatin1().constData(), player, player->playing, game_id);
#endif //RE_DEBUG
	player->playing = game_id;
	if(player->observing && player->observing != game_id)
	{
		GameListing * g = getDefaultRoomDispatch()->getGameListing(player->observing);
		if(g)
		{
			g->observers--;
			if(!g->observers)
			{
				//broadcast games never go down, so there's FIXME
				//issues here
				qDebug("Closing game %d, no observers", g->number);
				g->running = false;
				getDefaultRoomDispatch()->recvGameListing(g);	
			}
		}
	}
	else
		player->observing = game_id;
	/* Don't need to increment observers because this is done elsewhere, as room
	 * is created joined, or on the games list */
	GameListing * g = getDefaultRoomDispatch()->getGameListing(player->playing);
	//if(g)
	//	g->observers++;
}

//0x55c3
void TygemConnection::handleSetPhraseChatMsg(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	//2a02ffff 0000 0000 63ea000083cedc9
	unsigned short id = p[0] + (p[1] << 8);
	unsigned short directed_id;
	unsigned short setphrase_code;
	PlayerListing * player = getDefaultRoomDispatch()->getPlayerListing(id);
	if(!player)
	{
		qDebug("Can't get player for set chat msg\n");
		return;
	}
	/* Our chat appears back in all the rooms were in???
	 * why?? */
	//if(player->name == getUsername())
	//	return;		//to prevent duplicate chats from us
	directed_id = p[0] + (p[1] << 8);
	p += 4;
	p += 4;
/* They translated a lot more phrases, but these are the major ones
 * it looks like */
//p[1] byte is category? and then p[0] is specific msg	
	setphrase_code = p[0] + (p[1] << 8);
	std::map<unsigned short, QString>::iterator it = ORO_setphrase.find(setphrase_code);
	if(it != ORO_setphrase.end())
	{
		if(room_were_in)
		{
			BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
			if(!boarddispatch)
			{
				// might just be a room
				if(console_dispatch)
					console_dispatch->recvText(player->name + ": " + it->second);
			}
			else
				boarddispatch->recvKibitz(player->name, it->second);
		}
		else
		{
			if(console_dispatch)
				console_dispatch->recvText(player->name + ": " + it->second);
		}
	}
	else
	{
		if(room_were_in)
		{
			BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
			if(!boarddispatch)
			{
				if(console_dispatch)
					console_dispatch->recvText(player->name + ": " + QString::number(setphrase_code, 16));
			}
			else
				boarddispatch->recvKibitz(player->name, QString::number(setphrase_code, 16));
		}
		else
		{
			if(console_dispatch)
				console_dispatch->recvText(player->name + ": " + QString::number(setphrase_code, 16));
		}
	}
#ifdef RE_DEBUG
	printf("0x55c3: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}

//0xf659
void TygemConnection::handleServerAnnouncement(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	int i;
	//0xf659: 690b018b819f91e63289f191e598618fd88c949474836c8362836788cd8ce98341837d83608385834191498ee88ca0
	//819f82bd82ad82b382f182cc8a4682b382dc82c9834783938367838a815b92b882ab82dc82b582c482a082e882aa82c682a482
	//b282b482a282dc82b78149323093fa82c991ce8bc7916782dd8d8782ed82b982f094ad955c82a282bd82b582dc82b7814291e5
	//89ef82cc8fda8dd782e28e5189c18ff38bb582c882c782cd91e589ef90ea97708379815b835782c9838d834f8343839382cc8f
	//e382b28a6d944682ad82be82b382a281a8687474703a2f2f752d67656e2e6e69686f6e6b69696e2e6f722e6a702f6461697761
	//2f6130322f696e6465782e617370
#ifdef RE_DEBUG
	printf("0xf659: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	p += 2;

	/* FIXME, likely first two bytes are something else */
	QString u;
		
	u = serverCodec->toUnicode((const char *)p, size - 2);
			//u = codec->toUnicode(b, size - 4);
	if(console_dispatch)
		console_dispatch->recvText("*** " + u);
}

void TygemConnection::handleServerRoomChat(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned char * text = new unsigned char[size - 0x23];
	unsigned char name[11];
	QString encoded_name;
	//unsigned int text_size = (size - 2) / 2;
	//unsigned short * text = new unsigned short[text_size];
	unsigned short player_id;// = p[0] + (p[1] << 8);
	//00 58 06 17 BB C6 D1 A9 D1 C7 00 00 00 00 00 00  .X..............
	//00 00 00 04 7A 7A 7A 30 35 37 34 00 00 00 00 02  ....zzz0574.....
	//00 00 00 00 2D 00 FF FF 33 31 31 31 31 31 31 31  ....-...31111111
	//31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31  1111111111111111
	//31 31 31 31 31 31 31 31 31 31 31 31 31 31 31 31  1111111111111111
	//31 31 31 31 31 00 00 00                          11111...
	/*printf("serverchat: \n");
	for(int i = 0; i < size; i++)
		printf("%02x", msg[i]);
	printf("\n");*/
	//00 A8 06 17 D2 C0 D2 C0 D4 C2 D4 C2 00 00 00 00  ................
	//		00 00 00 00 71 71 35 32 32 32 00 00 00 00 00 02  ....qq5222......
	//		00 00 00 00 7E 00 FF FF 33 32 32 32 32 32 32 32  ....~...32222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 32 32 32 32 32 32 32 32 32 32  2222222222222222
	//		32 32 32 32 32 32 00 00                          222222..
	p += 4;
	strncpy((char *)name, (char *)p, 11); 
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	// FIXME really should be encoded name here I think... not ascii
	p += 0x0c;
	strncpy((char *)name, (char *)p, 11); 
	p += 0x14;
	//don't know what that one byte is, but its not part of text
	p++;
	strncpy((char *)text, (char *)p, size - 0x24);

	RoomDispatch * roomdispatch = getDefaultRoomDispatch(); //3
	//PlayerListing * player = roomdispatch->getPlayerListing(player_id);
	//if(player)
	//{
#ifdef RE_DEBUG
		printf("%s says: \n", name);
		for(int i = 0; i < (int)size - 0x24; i++)
			printf("%02x", text[i]);
		printf("\n");
#endif //RE_DEBUG
		QString u;
		
		u = serverCodec->toUnicode((const char *)text, strlen((char *)text));
			//u = codec->toUnicode(b, size - 4);
		if(console_dispatch)
			console_dispatch->recvText(QString((char *)name) + ": " + u);
	/*}
	else
		printf("unknown player says something");*/
	delete[] text;
}
//0x0661: 
			//0003 0001 0000 0000 0000 0000 0000 0000
			//0000 0002 696e 7472 7573 696f 6e00 0000
			//0000 0009 696e 7472 7573 696f 6e00 0002
			//0000 0000 0000 0000 0000 0000 0000 0000
			//0700 0000 3368 656c 6c6f 2000 0000 0000
			
			//0003 0001 0000 0000 0000 0000 0000 0000
			//0000 0001 6269 7473 0000 0000 0000 0000
			//0000 0014 6269 7473 0000 0000 0000 0002
			//0000 0000 0000 0000 0000 0000 8407 b900
			//0c00 0000 32ca f3b1 eabb acb6 afc1 cb20
			//0000 0000

//why
//00ab00010000000000000000000000000000000070657465726975730000000000000008706574657269757300
//00000002000000000000000000000000000000040000003377687920000000
//has garbled stuff on the end, maybe due to unicode, still FIXME
//0661
void TygemConnection::handleGameChat(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned char * text = new unsigned char[size - 3];
	unsigned char name[11];
	BoardDispatch * boarddispatch;
	int room_number;
	int size_of_message;
	QString encoded_name, ascii_name;
	QString rank;
#ifdef RE_DEBUG
	int i;
#endif //RE_DEBUG
	printf("** game chat size: %d: ", size);
	for(i = 0; i < (int)size; i++)
		printf("%02x", p[i]);
	printf("\n");
	//RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	room_number = (p[0] << 8) + p[1];
	boarddispatch = getIfBoardDispatch(room_number);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for chat msg", room_number);
		return;
	}
	p += 0x14;
	strncpy((char *)name, (char *)p, 11); 
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	p += 14;
	p++;
	if(p[0] < 0x12)
		rank = QString::number(0x12 - p[0]) + 'k';
	else if(p[0] > 0x1a)
		rank = QString::number(p[0] - 0x1a) + 'p';
	else
		rank = QString::number(p[0] - 0x11) + 'd';
	p++;
	strncpy((char *)name, (char *)p, 11); 
	ascii_name = QString((char *)name);
	p += 0x1c;
	size_of_message = p[0];
	if(size_of_message > size - 66)
	{
		qDebug("Bad in game chat message size");
		return;
	}
	p += 4;
	//again, init number not part of text
	p++;
	strncpy((char *)text, (char *)p, size_of_message);
	
	//PlayerListing * player = roomdispatch->getPlayerListing(player_id);
	//if(player)
	//{
	
	QString u;
		
	u = serverCodec->toUnicode((const char *)text, strlen((char *)text));
			//u = codec->toUnicode(b, size - 4);
	boarddispatch->recvKibitz(encoded_name + "[" + rank + "]", u);
	/*}
	else
	printf("unknown player says something");*/
	delete[] text;
}

//069f: 
//069f
//7065 7465 7269 7573 0000 0000 0000 0000
//696e 7472 7573 696f 6e00 0000 0000 0009
//696e 7472 7573 696f 6e00 0002 0001 0000
//0c68 656c 6c6f 2074 6865 7265 0000 0000
//you know; testing one more time
//069f 
//7065 7465 7269 7573 0000 0000 0000 0000 
//696e 7472 7573 696f 6e00 0000 0000 0009
//696e 7472 7573 696f 6e00 0002 0009 796f
//7520 6b6e 6f77 0000 1674 6573 7469 6e67
//206f 6e65 206d 6f72 6520 7469 6d65 0000
/* I think these are actually supposed to be little email type msgs even though
 * they're small and conversations are supposed to be held over
 * something else that we have to respond to to open up */
void TygemConnection::handlePersonalChat(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned char * text = new unsigned char[size - 7];
	int i;
	unsigned char name[11];
	int subject_size, msg_size;
	QString our_name, encoded_name, ascii_name, rank, subject;
#ifdef RE_DEBUG
	printf("** msg size: %d: ", size);
	for(i = 0; i < (int)size; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	strncpy((char *)name, (char *)p, 11);
	our_name = QString((char *)name);
	p += 0x10;
	strncpy((char *)name, (char *)p, 11);
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	p += 0xd;
	p++;
	//7065 7465 7269 7573 0000 0000 0000 0000
	//696e 7472 7573 696f 6e00 0000 0000 0009
	//696e 7472 7573 696f 6e00 0002 0003 6161
	//0000 0b61 6261 6261 6220 6162 6100 0000
	//rank byte
	if(p[0] < 0x12)
		rank = QString::number(0x12 - p[0]) + 'k';
	else if(p[0] > 0x1a)
		rank = QString::number(p[0] - 0x1a) + 'p';
	else
		rank = QString::number(p[0] - 0x11) + 'd';
	p++;		
	strncpy((char *)name, (char *)p, 11);
	ascii_name = QString((char *)name);
	p += 10;
	p += 4;
	subject_size = p[0];
	p++;
	printf("Subjectsize: %d\n", subject_size);
	fflush(stdout);
	fflush(stderr);
	strncpy((char *)text, (char *)p, subject_size);
	subject = QString((char *)text);
	p += subject_size;
	msg_size = (p[0] << 8) + p[1];		//tygem caps at 1024
	p += 2;
	printf("msg_size: %d\n", msg_size);
	fflush(stdout);
	fflush(stderr);
	strncpy((char *)text, (char *)p, msg_size + 1);
	
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	const PlayerListing * player = roomdispatch->getPlayerListing(encoded_name);
	
	if(player)
	{
#ifdef RE_DEBUG
		printf("%s says to you ", player->name.toLatin1().constData());
		for(i = 0; i < (int)size - 8; i++)
			printf("%02x", text[i]);
		printf("\n");
#endif //RE_DEBUG
		QString u;
		
		u = serverCodec->toUnicode((const char *)text, strlen((char *)text));
		TalkDispatch * talk = getTalkDispatch(*player);
		if(talk)
		{
			talk->recvTalk(u);
			talk->updatePlayerListing();
		}
		//FIXME highlight or something
		//if(console_dispatch)
		//	console_dispatch->recvText(player->name + "-> " + QString((char *)text));
	}
	else
		printf("unknown player says something");
	delete[] text;
}


//not disconnect, enter lobby? 0e56, really leaning towards enterlobby
// or enter anything
// this is definitely an actual enter room.  We might need to use this
// when we join a room, especially or in particular a broadcast room
// if the betting message doesn't otherwise indicate that we've actually
// joined.  Otherwise we get the move list and there's nothing tohandle it
// FIXME
void TygemConnection::handlePlayerRoomJoin(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	unsigned int id;
	unsigned short room_number;
	BoardDispatch * boarddispatch;
	GameListing * game;
	
	if(size != 4)
		qDebug("handlePlayerRoomJoin of size: %d\n", size);
	id = p[0] + (p[1] << 8);
	PlayerListing * aPlayer = roomdispatch->getPlayerListing(id);
	if(!aPlayer)
	{
		printf("Can't find msg player %02x%02x\n", p[0], p[1]);
		return;
	}
	room_number = p[2] + (p[3] << 8);
	if(id == our_player_id)
		setRoomNumber(room_number);
#ifdef RE_DEBUG
	printf("%s %02x%02x entering room %d\n", aPlayer->name.toLatin1().constData(), p[0], p[1], room_number);
#endif //RE_DEBUG
	/* We get this message before the boarddispatch is created, which
	 * means that we won't see ourselves entering the game.  If
	 * we wanted to fix this, we could create the boarddispatch
	 * possibly a little earlier, or just add our own name as
	 * a listing when we observe games.  Then there's matches
	 * also. FIXME */
	if(room_were_in)
	{
		if(aPlayer->observing == room_were_in)
		{
			boarddispatch = getIfBoardDispatch(room_were_in);
			if(boarddispatch)
				boarddispatch->recvObserver(aPlayer, false);
		}
		else if(room_number == room_were_in)
		{
			boarddispatch = getIfBoardDispatch(room_were_in);
			if(boarddispatch)
				boarddispatch->recvObserver(aPlayer, true);
		}
	}
	if(aPlayer->observing)
	{
		game = roomdispatch->getGameListing(aPlayer->observing);
		if(game)
		{
			game->observers--; 
			if(game->observers == 0)
			{
				/* We should probably increment an observer count here, and
				* decrement, and if it gets to 0, remove the room.  I don't
				* think there's another message that does it FIXME (but this
				* requires accurate observer counts. */
				game->running = false;
				roomdispatch->recvGameListing(game);	
			}
		}
	}
	aPlayer->observing = room_number;
	game = roomdispatch->getGameListing(room_number);
	if(game)
		game->observers++; 
	
	/* We set to 0 here with the idea that they aren't entering
	 * their own game, but its a little weird */
	setAttachedGame(aPlayer, 0);
	roomdispatch->recvPlayerListing(aPlayer);
	// This can definitely be the number of a game, not the game_code
	
	//aPlayer->online = false;
	//roomdispatch->recvPlayerListing(aPlayer);
}

//4a9c likely real disconnect or maybe not!!!
void TygemConnection::handlePlayerDisconnect2(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	GameListing * game;
	unsigned char * p = msg;
	unsigned int id;
	unsigned short room_id;
	if(size != 4)
		qDebug("handlePlayerDisconnect of size: %d\n", size);
	id = p[0] + (p[1] << 8);
	PlayerListing * aPlayer = roomdispatch->getPlayerListing(id);
	if(!aPlayer)
	{
		printf("Can't find msg player %02x%02x\n", p[0], p[1]);
		return;
	}
	p += 2;
	/* If this is the room id, its reversed... */
	room_id = p[0] + (p[1] << 8);
#ifdef RE_DEBUG
	printf("%s 4a9c: %02x%02x %d %d\n", aPlayer->name.toLatin1().constData(), p[0], p[1], room_id, aPlayer->observing);
#endif //RE_DEBUG
	if(1)
	{
		if(aPlayer->observing)
		{
			game = roomdispatch->getGameListing(aPlayer->observing);
			if(game)
			{
				game->observers--; 
				if(game->observers == 0)
				{
					game->running = false;
					roomdispatch->recvGameListing(game);	
				}
			}
		}
		aPlayer->online = false;
		//way too many
		//QMessageBox::information(0 , tr("disconnecting"), aPlayer->name);

		roomdispatch->recvPlayerListing(aPlayer);
		
	}
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_id);
	if(boarddispatch)
	{
		// Player left a running game?  maybe observer though
		// but we need to check if stop clock is necessary
		boarddispatch->recvKibitz(QString(), QString("%1 has left the room.").arg(aPlayer->name)); 
	}
}

//85bb
// I think these are result messages... I thought they were new game
// from people doing rematches... but there's no game codes in them...
// they could also be player updates I guess, but that's doubtful
void TygemConnection::handleNewGame(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * player;
	unsigned short player_id = p[0] + (p[1] << 8);
	int i;
	if(size != 28)
		qDebug("85bb new game of size %d", size);
#ifdef RE_DEBUG
	printf("Game result msg?: ");
#endif //RE_DEBUG
	player = roomdispatch->getPlayerListing(player_id);
#ifdef RE_DEBUG
	if(player)
		printf("%s ", player->name.toLatin1().constData());
	// 4th byte here is 01 for winner and 02 for loser
	// separate msg for each player result
	if(p[3] == 0x10)
		printf(" wins ");
	else if(p[3] == 0x20)
		printf(" loses ");
	else
		printf(" %02x ", p[3]);
	for(i = 0; i < 28; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	// we probably don't send these during pass?
	if(player_id == our_player_id)
		killActiveMatchTimers();
	//GameListing * g = roomdispatch->getGameListing();
	//g->running = false;
	//roomdispatch->recvGameListing(g);
}

//0666
void TygemConnection::handleObserverList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;

	unsigned int game_number;
	unsigned short id;
	unsigned short number_of_observers;
	GameListing * game;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	PlayerListing * aPlayer;
	//PlayerListing * newPlayer = new PlayerListing();
	QString encoded_name, ascii_name, rank;
	int country_size;
	unsigned char name[20];		//country can be larger than 11
	game_number = (p[0] << 8) + p[1];
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("no board dispatch for observer list");
		return;
	}
	//0x0667: 0002 0001 0002 009e 696e 7472 7573 696f 
	//        6e00 0000 0000 0009 696e 7472 7573 696f
	//        6e00 0002 0000 0000 0000 0000 0000 0000
	//        0000 0000 0000 0005 6368 696e 61

	//0x0667: 0002 0001 0100 0000 696e 7472 7573 696f 6e00 0000 0000 0009
	//00ec 0666 00fa 0004 
	//0002 
	//009e b9ed c3c5 b9d8 0000 0000 0000 0000 
	//0014 676d 6732 3030 3500 3000 0002 0000
	//0000 0000 039d 0000 0391 0000 
	//0003 0000 0005 6368 696e 61
	//00 0200 9e69 6e74 7275
	//7369 6f6e 0000 0000 0000 0969 6e74 7275
	//7369 6f6e 0000 0200 0000 0000 0000 0000
	//0000 0000 0000 0000 0000 0563 6869 6e61
	
	//0002 009e 7065 7465 7269 7573 0000 0000
	//0000 0008 7065 7465 7269 7573 0000 0002
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0005 6368 696e 61
	
	//00 0200 9e62 6c61
	//636b 736d 6972 6b00 0000 0000 1462 6c61
	//636b 736d 6972 6b00 0200 0000 0000 0001
	//2500 0001 2a00 0000 0400 0000 0563 6869
	//6e61
	//gmg2005, intrusion, peterius, blacksmirk, china china china china
	
	
	//0002 009e 7065 7465 7269 7573 0000 0000
	//0000 0008 7065 7465 7269 7573 0000 0002
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0005 6368 696e 61
	//00 0200 00b1 a8b9
	//facb c2b6 b9cb bf00 0000 0000 1242 4753
	//444f 5553 0000 0000 0200 0000 0000 0000
	//0d00 0000 0000 0000 0100 0000 0c63 6869
	//6e61 5f62 616f 6775 6f
	//00 0200 9ec8 f1d3
	//a500 0000 0000 0000 0000 0000 126a 7866
	//006c 6f70 6500 0000 0200 0000 0000 000d
	//2000 000e a800 0000 0700 0000 0563
	p += 2;
	number_of_observers = (p[0] << 8) + p[1];
	p += 2;
	printf("observers %d:\n", number_of_observers);
	for(int i = 0; i < size; i++)
		printf("%02x", p[i]);
	printf("\n");
	/* FIXME possible issue with country name which is not included in
	 * 0x34 */
	while(p < (msg + size - 0x34) || (p < (msg + size - 19) && p[0] == 0x01))
	{
		if(p[0] == 0x01)
		{
			//0100 0000 696e 7472 7573 696f 6e00 0000 0000 0009
			p += 4;
			strncpy((char *)name, (char *)p, 11);
			encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
			aPlayer = roomdispatch->getPlayerListing(encoded_name);
			if(aPlayer)
			{
				boarddispatch->recvObserver(aPlayer, false);
			}
			else
				qDebug("Can't find disconnecting player");
			p += 15;
			printf("Player %s leaving room, last byte: %02x\n", encoded_name.toLatin1().constData(), p[0]);
			p++;
			printf("Found leaving observer\n");
			continue;
		}
		if(p[1] == 0x39)
		{
			p += 4;
			printf("observer, what the hell is this\n");
			continue;
		}
		else if(p[1] != 0x02)
		{
			printf("%02x%02x\n", p[0], p[1]);
		}
		for(int i = 0; i < 0x34; i++)
			printf("%c", p[i]);
		printf("\n");
		p += 2;
		p++;
		if(!p[0])
		{
			printf("Observer Guest\n");
			//0002 0000 5b47 3337 3039 5d32 3000 0000
			//0000 0000 5b47 3337 3039 5d32 3000 0000
			//0000 0100 0000 0000 0000 0000 0000 0000
			//0000 0000
			p += 48;
			p += p[0] + 1;
			continue;
		}
		//0002 019d 7866 6c73 6473 0000 0000 0000
		//0000 000f 7866 6c73 6473 006c 6400 0002
		//0000 0000 0000 00af 0000 00d3 0000 0001
		//0000 000d 6368 696e 615f 6875 6469 6567
		//75
		//0002 015e cedd e9dc 0000 0000 0000 0000
		//0000 0013 3139 3732 3035 0030 3000 0002
		//0000 0000 0000 038c 0000 033e 0000 0003
		//0000 000c 6368 696e 615f 6865 6962 6169
		
		/* I think p[0] is the flag icon, but it can be different
		* pictures too so... */
		p++;
		strncpy((char *)name, (char *)p, 11);
		//encoded_name = QString((char *)name);
		/* WARNING There's a chance that encoded names are not one
		 * to one and since the names are used to look players up,
		 * doing toUnicode could write to the wrong player.
		 * possible FIXME*/
		encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
		p += 14;
		p++;
		//0002 ffff 6a75 6e67 616e 6732 3000 0000
		//0000 0012 6a75 6e67 616e 6732 3000 0000
		//0000 0000 0000 0152 0000 0171 0000 0004
		//0000 0000
		//rank byte
		if(p[0] < 0x12)
			rank = QString::number(0x12 - p[0]) + 'k';
		else if(p[0] > 0x1a)
			rank = QString::number(p[0] - 0x1a) + 'p';
		else
			rank = QString::number(p[0] - 0x11) + 'd';
		p++;
		strncpy((char *)name, (char *)p, 11);
		ascii_name = QString((char *)name);
		//another name
		aPlayer = roomdispatch->getPlayerListing(encoded_name);
		if(!aPlayer)
		{
			//aPlayer = newPlayer;
			printf("Can't find observer: %s in full list\n", ascii_name.toLatin1().constData());
			p += 31;
			p += p[0] + 1;
			continue;
		}
		//unnecessary to set the name since we can't get
		//record without it... probably ascii_name as well
		//FIXME, but if we didn't need player lists... 
		//technically we should be able to display anyway...
		//aPlayer->name = encoded_name;
		//aPlayer->ascii_name = ascii_name;
		//aPlayer->rank = rank;
		p += 8;
		p += 4;
		p += 16;
		p += 2;
		p++;
		
		/* Below is player listing packet */
		//0002 009e 3737 3435 3532 0000 0000 0000 
		//0000 0000 3737 3435 3532 006f 6f6e 0002
		//0000 0000 0000 0064 0000 012b 0000 0000
		//ffff ff40 0001 004b
		/****/
		/* And observer listing */
		//0002 009e 7065 7465 7269 7573 0000 0000
		//0000 0008 7065 7465 7269 7573 0000 0002
		//0000 0000 0000 0000 0000 0000 0000 0000
		//0000 0005 6368 696e 61
		
		country_size = p[0];
		p++;
		if(country_size > 19)
		{
			qDebug("Strange country name size greater than hard coded array!!!");
			p += country_size;
			continue;	
		}
		strncpy((char *)name, (char *)p, country_size);
		name[country_size] = '\0';
		p += country_size;
		aPlayer->country = QString((char *)name);
		/*recvObserver does not make a copy at any point.
		 *its supposed that the player listing comes from
		 *the full listing and will be handled with that.*/
		
		boarddispatch->recvObserver(aPlayer, true);
		if(playing_game_number == game_number && aPlayer == player_accepted_match)
		{
			/* They've just joined our created room
			 * so we send the rest of this.  Presuming
			 * its not a resume... FIXME */
			sendMatchMsg1(*player_accepted_match, game_number);
			MatchRequest * mr = new MatchRequest();
			mr->number = game_number;
			mr->opponent = player_accepted_match->name;
			mr->opponent_is_challenger = false;
			mr->first_offer = true;
			mr->timeSystem = byoyomi;
			
			GameDialogDispatch * gd = getGameDialogDispatch(*player_accepted_match);
			gd->recvRequest(mr, getGameDialogFlags());
			delete mr;
		}
	}
	if(p != (msg + size))
	{
		qDebug("handleObserverList strange size: %d", (msg + size) - p);
	}
	
	//delete newPlayer;
}

//0xe1af:
void TygemConnection::handleGameEnded(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	unsigned short game_code;
	unsigned int game_number;
	GameListing * game;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	if(size != 10)
	{
		qDebug("GameEnded of strange size: %d", size);
		return;
	}
	game_code = p[0] + (p[1] << 8);
	game_number = p[2] + (p[3] << 8);
	p += 4;
	//DOUBLECHECK
	game = roomdispatch->getGameListing(game_number);
	if(!game)
	{
		qDebug("e1af for no game\n");
		return;
	}
	//our_game_being_played is no longer viable here because of handleScore
	/* FIXME, what about owner_id???  this could be a room
	 * closed message */
	/* Room doesn't go away after game, one can still join it until
	 * everyone is gone */
#ifdef NOTREALLY
	if(game->white)
	{
		game->white->playing = 0;
		game->white = 0;	//just in case
	}
	if(game->black)
	{
		game->black->playing = 0;
		game->black = 0;	//just in case
	}
	game->running = false;
	roomdispatch->recvGameListing(game);
	game_code_to_number.erase(game_number);	// good?
	/* To help clear out 0a7d records */
	clearRoomsWithoutGames(game_number);	//this is where it was to start with
#endif //NOTREALLY
	//the rest are pretty repetitive, non-specific FIXME
#ifdef RE_DEBUG
	printf("0xe1af: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}

/* I'm thinking 0a7d may be NewMatch and ca5d is NewRoom FIXME */
//0a7d
void TygemConnection::handleNewRoom(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * black;
	PlayerListing * white;
	unsigned short id;
	unsigned short number;
	if(size != 6)
	{
		qDebug("NewRoom of strange size: FIXME 18 %d", size);
		//FIXME we're having handleResume call this with size 18
		// fix that... 
		//return;
	}
	GameListing * newGameListing = new GameListing();
	GameListing * aGameListing;
	newGameListing->running = true;
	
	number = p[0] + (p[1] << 8);
	if(!number)
		qDebug("0a7d on number 0");
	aGameListing = roomdispatch->getGameListing(number);
	if(!aGameListing)
		aGameListing = newGameListing;
	aGameListing->number = number;
	p += 2;
	/* Why does ORO 0a7d have white then black, but 1a81 has
	 * black then white?  And what about numbers and game_codes?
	 * it seems like ORO protocol is based on two different programs,
	 * like there's legacy code in there.  And I complained about IGS
	 * so much.  I guess nothing is perfect.  And I'm lazy in this all
	 * the time too...
	 * Okay, actually, this seems to reverse, like maybe this just means
	 * they're in the same room or something and they're still deciding
	 * white versus black, or there's some nigiri thing... this white
	 * black DOESN'T matter. */
	/* Okay, since we changed the listing to handle numbers, game codes
	 * on board dispatches... what's changed here ??? FIXME */
	aGameListing->owner_id = p[0] + (p[1] << 8);
	white = roomdispatch->getPlayerListing(aGameListing->owner_id);
	if(!white)
	{
		printf("can't get white player: %02x%02x\n", p[0], p[1]);
		return;
	}
	else
	{
		/* This is redundant, but we won't receive entering msgs
		 * for these players */
		setAttachedGame(white, aGameListing->number);
		white->observing = aGameListing->number;
		if(aGameListing->owner_id == our_player_id)
			setRoomNumber(aGameListing->number);
	}
	p += 2;
	id = p[0] + (p[1] << 8);
	black = roomdispatch->getPlayerListing(id);
	if(!black)
	{
		printf("can't get black player: %02x%02x\n", p[0], p[1]);
		//return;		//not an issue
	}
	else
	{
		setAttachedGame(black, aGameListing->number);
		black->observing = aGameListing->number;
		if(id == our_player_id)
			setRoomNumber(aGameListing->number);
	}
	/* FIXME, no recv, no game yet, maybe there should be. */
	aGameListing->white = white;
	aGameListing->black = black;
	//aGameListing->observers = 1;	//the person who created it
#ifdef RE_DEBUG
	printf("0a7d for game with no game code: %d %s %s\n", aGameListing->number, white->name.toLatin1().constData(), black->name.toLatin1().constData());
#endif //RE_DEBUG
	roomdispatch->recvGameListing(aGameListing);
	//FIXME test without delete
	//delete newGameListing;
	aGameListing = roomdispatch->getGameListing(number);
	//rooms_without_games.push_back(aGameListing);
}

//1a81
void TygemConnection::handleGameMsg(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	unsigned short game_code;
	unsigned int handicap;
	unsigned char byte;
	PlayerListing * black;
	PlayerListing * white;
	PlayerListing * owner;
	GameListing * aGameListing = 0;
	int i;
#ifdef RE_DEBUG
	printf("1a81 message: ");
#endif //RE_DEBUG
	game_code = p[0] + (p[1] << 8);		//used in joining games
#ifdef RE_DEBUG
	printf("%02x%02x {%02x ", p[0], p[1], p[2]);
#endif //RE_DEBUG
	p += 2;
	//0x1f
	p++;
	handicap = p[0];
	p++;
#ifdef RE_DEBUG
	for(i = 0; i < (int)size - 4; i++)
		printf("%02x", p[i]);
#endif //RE_DEBUG
	byte = p[2];
	p += 2;
	
	black = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
#ifdef RE_DEBUG
	if(black)
		printf("} game of %s %02x%02x", black->name.toLatin1().constData(), p[0], p[1]);
#endif //RE_DEBUG
	p += 2;
	white = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
#ifdef RE_DEBUG
	if(white)
		printf(" and %s %02x%02x", white->name.toLatin1().constData(), p[0], p[1]);
#endif //RE_DEBUG
	p += 2;
#ifdef RE_DEBUG
	printf("\n");
#endif //RE_DEBUG
	if(!black || ! white)
	{
		printf("%d gamecode game is missing black or white\n", game_code);
		return;
	}
	/* If there's a rematch, we might be able to just use
	 * the "attachedgame" to get the number */
	if(black->observing)
	{
		aGameListing = roomdispatch->getGameListing(black->observing);
#ifdef RE_DEBUG
		if(aGameListing)
			printf("Found %d for this game\n", aGameListing->number);
#endif //RE_DEBUG	
	}
	if(white->observing && !aGameListing)
	{
		aGameListing = roomdispatch->getGameListing(white->observing);
#ifdef RE_DEBUG
		if(aGameListing)
			printf("Found %d for this game\n", aGameListing->number);
#endif //RE_DEBUG
	}
	if(black->playing && !aGameListing)
	{
		aGameListing = roomdispatch->getGameListing(black->playing);
#ifdef RE_DEBUG
		if(aGameListing)
			printf("Found %d for this game\n", aGameListing->number);
#endif //RE_DEBUG
	}
	if(white->playing && !aGameListing)
	{
		aGameListing = roomdispatch->getGameListing(white->playing);
#ifdef RE_DEBUG
		if(aGameListing)
			printf("Found %d for this game\n", aGameListing->number);
#endif //RE_DEBUG
	}
	if(aGameListing && aGameListing->running == false)
	{
		qDebug("Found non running game in game registry !!!");
		return;
	}
#ifdef OLD
	if(!aGameListing)
	{
	/* Messy, but hopefully integrated 0a7d and 1a81 messages FIXME */
	/* Player colors don't matter on 0a7d, probably chat room join,
	 * unconnected.  Also be careful of potential to leave chat
	 * rooms ?!?! as well as where is the start of the room? */
	std::vector<GameListing *>::iterator pl_i;
	for(pl_i = rooms_without_games.begin(); pl_i != rooms_without_games.end(); pl_i++)
	{
		/*if(((*pl_i)->black == black &&
		    (*pl_i)->white == white) ||
		   ((*pl_i)->black == white &&
	            (*pl_i)->white == black))*/
		if(( ((*pl_i)->owner_id == black->id ||
		   (*pl_i)->owner_id == white->id) ||
			((*pl_i)->number == black->observing ||
			(*pl_i)->number == white->observing)) )
		{
			aGameListing = *pl_i;
			rooms_without_games.erase(pl_i);
			printf("Found %d for this game\n", aGameListing->number);
			PlayerListing * owner = roomdispatch->getPlayerListing(aGameListing->owner_id);
			setAttachedGame(owner, 0);
			break;
		}
	}
	}
#endif //OLD
	if(!aGameListing)
	{
		printf("No 0a7d for this game\n");
		return;
		/* FIXME, if there's no room, we can't add the game. */
	}
	else
	{
		//detach owner if any, players are reattached
		owner = roomdispatch->getPlayerListing(aGameListing->owner_id);
		if(owner && owner != black && owner != white)
			setAttachedGame(owner, 0);
		/* FIXME we may want to swap the colors? if they're now
		 * legit, pending on nigiri comprehension?? */
	}
	aGameListing->black = black;
	aGameListing->white = white;
	if(owner != black)
		setAttachedGame(black, aGameListing->number);
	if(owner != white)
		setAttachedGame(white, aGameListing->number);
	aGameListing->FR = QString::number(byte);		//should probably just be ->flags FIXME
	aGameListing->game_code = game_code;
	aGameListing->handicap = handicap;
	aGameListing->isRoomOnly = false;
	
	//how is it necessary to do recv here if we got the listing previously?
	//roomdispatch->recvGameListing(aGameListing);
	//FIXME test without delete
	//delete aGameListing;
}
//1b00 2900 0100 0000 0600 2c01 6900 2500 0404 0000 1400 0400 0200 6b61 7365 697a 696e 0000
//626d 7762 6d77 0000 0000 6b61 7365 697a 696e 0000 8df7 92ac 0000 0000 0000 0303 a8a8 0103
//000200000000000042000000000000000000000000000000000000000000000000000000000000000000000b

//1b00 ffff 0100 0000 0600 8403 8403 8403 0303 0000 1e00 0300 0200 7363 6879 3837 3337 0000
//7968 6c65 6564 6562 0000 bcf8 bcf6 0000 0000 0000 c8ab c0ba b0c5 bbe7 0000 0101 a7a7 0105 
//000200000000000000000000000000000000000000000000000000000000000000000000000000000001000b


//1c00 ffff 0100 0000 0600 2c01 2c01 2c01 0505 0000 1400 0500 0200 7475 7266 6679 0000 0000
//6c69 6831 3230 3600 0000 8b49 9656 0000 0000 0000 3132 3036 0000 0000 0000 0301 a7a7 0105
//000200000000000000000000000000000000000000000000000000000000000000000000000000000001000b

//1c00 ffff 0100 0000 0600 2c01 2c01 2c01 0303 0000 1e00 0300 0200 696e 796f 7000 0000 0000
//6b6b 3036 3239 0000 0000 c7d1 bdb4 c7d1 bdb4 0000 6b6b 3036 3239 0000 0000 0101 a7a7 0101
//00020000000000000000000000000000000000000000000000000000000000000000000000000
//0000001000b


/* I think we get these when we try to observe broadcasted, or betting games
 * as well. */
void TygemConnection::handleBettingMatchStart(unsigned char * msg, unsigned int size)
{
}

//ca5d	chat rooms also
//this could potentially also be a join room
void TygemConnection::handleCreateRoom(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * aPlayer;
	unsigned short room_number;
	unsigned char room_type;
	int i;
	
	aPlayer = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
#ifdef RE_DEBUG
	if(aPlayer)
		qDebug("player %s %02x%02x chat room/match\n", aPlayer->name.toLatin1().constData(), p[0], p[1]);
	else
		qDebug("can't find player %02x%02x\n", p[0], p[1]);
#endif //RE_DEBUG
	p += 2;
	room_number = p[0] + (p[1] << 8);
	GameListing * aGameListing = new GameListing();
	aGameListing->running = true;
	aGameListing->number = room_number;
	aGameListing->owner_id = aPlayer->id;
	aGameListing->white = aPlayer;
	
	aGameListing->black = 0;
	aGameListing->_black_name = QString::number(room_type, 16);
	aGameListing->observers = 1;	//the person who created it
	
	aGameListing->isRoomOnly = true;
#ifdef RE_DEBUG
	printf("room number on that chat: %d\n", room_number);
#endif //RE_DEBUG
	p += 2;
	p += 3;
	//probably room info
	//0000 00
	//4011
	switch(p[0])
	{
		case 0xc0:
			break;
		case 0x40:
			break;
		case 0x09:
			break;
		case 0x00:
			aGameListing->_black_name = "** wished " + QString::number(p[1], 16);
			break;
	}
	room_type = p[0];
	//p[1] here is time and strength wished as per create room code
	p += 2;
	p += 8;
	//a room byte info?
	p++;
	//title!!
	
	p += 20;
	// and lots of zeroes
	p += 22;
	//if(p != msg + size)
	//{
#ifdef RE_DEBUG
		printf("chat room strange size: %d\n", size);
		for(i = 0; i < (int)size; i++)
			printf("%02x", msg[i]);
		printf("\n");
#endif //RE_DEBUG
	//}
//review game minminmin 7d
//c901de00 0000 00c0 000000027d60005acdabbadc003b00003cb3a5017d60b65a68b3a501d98bcf7700e0fd7f68b3a5015a88cf770000000000000000
//chat room
//89041401 0000 0040 11000000000000000000000000760000000000000000000000000000000000000000000000000000000000000000000000000000
//chat room
//15008500 0000 0000 11000000000000000000000000b90000000000000000000000000000000000000000000000000000000000000000000000000000
//980d6600 0000 0000 11000000000000000000000000bc0000000000000000000000000000000000000000000000000000000000000000000000000000
//wished normal time even 1k
//74099800 0000 0000 11000000000000000000000000e90000000000000000000000000000000000000000000000000000000000000000000000000000
//wished normal time strong
//b8052301 0000 0000 01000000000000000000000000860000000000000000000000000000000000000000000000000000000000000000000000000000
//wished normal time even
//8d00b100 00000000 11000000000000000000000000dd0000000000000000000000000000000000000000000000000000000000000000000000000000
//wished normal time even
//29093201 00000000 110000000000000000000000008e0000000000000000000000000000000000000000000000000000000000000000000000000000
//74099800 00000000 11000000000000000000000000e90000000000000000000000000000000000000000000000000000000000000000000000000000
//quit playing
//5d0da60000000000 110000000000000000000000005b0000000000000000000000000000000000000000000000000000000000000000000000000000
//locked gomoku? equal standard wished?		
//6d0b1201 0000 0009 11000000000000000000000000ed0000000000000000000000000000000000000000000000000000000000000000000000000000	
	
	
	
	roomdispatch->recvGameListing(aGameListing);
	
	/* Set the attachment right after the game list is recvd */
	if(aPlayer)
		setAttachedGame(aPlayer, aGameListing->number);
	//FIXME test without delete
	delete aGameListing;
	aGameListing = roomdispatch->getGameListing(room_number);
	//printf("Pushing back %p with %d\n", aGameListing, aGameListing->number);
	//rooms_without_games.push_back(aGameListing);
}

//56f4
void TygemConnection::handleBettingMatchResult(unsigned char * msg, unsigned int size)
{
	unsigned short game_code = msg[0] + (msg[1] << 8);
	unsigned short game_id = game_code_to_number[game_code];
	int i;
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	GameListing * aGameListing = roomdispatch->getGameListing(game_id);
	
#ifdef RE_DEBUG
	printf("56f4: likely betting match result: %d\n", game_id);
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	
	
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_id);
	if(!boarddispatch)
		return;
	GameResult aGameResult;
	
	//4d75 0202 302e 3500 0000 0000 0000 0015
	/* FIXME */
	/* Seriously, fix this... score too */
	aGameResult.result = GameResult::RESIGN;
	boarddispatch->recvResult(&aGameResult);
	
	
	
	/* Sure they never hang around with the game over thing ? */
	//aGameListing->running = false;
	//roomdispatch->recvGameListing(aGameListing);
	//game_code_to_number.erase(game_id);
}

//5ac3
void TygemConnection::handleGamePhaseUpdate(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	GameListing * aGameListing;
	int i;
	if(size != 4)
	{
		qDebug("GamePhaseUpdate of strange size: %d", size);
		return;
	}
#ifdef RE_DEBUG
	printf("5ac3: %d fl %02x ", p[0] + (p[1] << 8), p[2]);
	for(i = 0; i < 4; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	// maybe first byte is game number?, and then update?
	/* I'm going to pretend these are game phase status messages */
	aGameListing = roomdispatch->getGameListing(p[0] + (p[1] << 8));
	if(aGameListing)
	{
		aGameListing->moves = getPhase(p[2]);
		roomdispatch->recvGameListing(aGameListing);
	}
	else
	{
		qDebug("Received 5ac3 for non existent game/room\n");
	}
}

//50c3
void TygemConnection::handleMsg3(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * player;
	int i;
#ifdef RE_DEBUG
	printf("50c3 size: %d\n", size);
	printf("**** 50c3: ");
	for(i = 0; i < 4; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
}

/* Maybe this is nigiri but it seems sent by other people's games
 * as well at strange times */
//f5af
void TygemConnection::handleNigiri(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * player;
	unsigned char thebyte;
	int i;
	unsigned short game_code = p[0] + (p[1] << 8);
	unsigned short room_number = p[2] + (p[3] << 8);
	QString name, rank, our_rank;
	GameData * gr;
	
	p += 4;
	thebyte = p[0];
	/* Its also possible that we'd need to do this on games that
	 * we joined.  Its even possible that this is sent all
	 * the time for games that just started */
	if(room_number == our_game_being_played)
	{
		if(we_send_nigiri)	//ignore
			return;
		//QMessageBox::information(0 , tr("receiving niri"), tr("receiving ") + QString::number(thebyte, 16) + tr(" nigiri ") + QString::number(p[3], 16));
		BoardDispatch * boarddispatch = getIfBoardDispatch(room_number);
		if(!boarddispatch)
		{
			qDebug("handlenigiri but no board dispatch");
			return;
		}
		/* "our game being played"  That's got to be the worst
		 * variable name in the history of the universe.
		 * even "our match in progress" would be slightly better.
		 * Or "number of current game", "currently played game number"... */
		
		/* We need to start the timers here I think */
		gr = boarddispatch->getGameData();
		
		//if((thebyte % 2) == 0)
		/* I'm thinking that we always get the one meant
		 * for us... that the server adds 1 or something
		 * to tailor it for us */
		if(((((thebyte & 0xe0) >> 5) + 1) % 2) == ((thebyte & 0x1f) % 2))	    
		{
			qDebug("nigiri successful");
			boarddispatch->recvKibitz(QString(), "Opponent plays black");
			if(gr->black_name == getUsername())
				boarddispatch->swapColors();
			else
				boarddispatch->swapColors(true);
		}
		else
		{
			qDebug("nigiri failure");
			boarddispatch->recvKibitz(QString(), "Opponent plays white");
			if(gr->white_name == getUsername())
				boarddispatch->swapColors();
			else
				boarddispatch->swapColors(true);
		}
		gr = boarddispatch->getGameData();	//get again
		startMatchTimers(gr->black_name == getUsername());
	}
	else
	{
		/* We get these a lot on other games and I think they
		 * may be part of starting a match... like we should
		 * check the owner and swap the colors if that's an issue
		 * FIXME */
	}
#ifdef RE_DEBUG
	printf("0xf5af: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}

void TygemConnection::handleMsg2(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	PlayerListing * player;
	int i;
	int records = p[0] + (p[1] << 8);
	p += 2;
#ifdef RE_DEBUG
	printf("Its msg2: e461\n");
#endif //RE_DEBUG
	//each record is 18 bytes;
	while(records--)
	{
#ifdef RE_DEBUG
		for(i = 0; i < 18; i++)
			printf("%02x", p[i]);
		printf("\n");
#endif //RE_DEBUG
		p += 18;
	}
	if(p != (msg + size))
		printf("Weird msg length: %d at line %d\n", size, __LINE__);
}

//0x69c3:
void TygemConnection::handleRematchRequest(unsigned char * msg, unsigned int size)
{
	/* In ORO win client this actually looks just like a matchinvite */
	if(size != 6)
	{
		qDebug("Rematch request of strange size: %d", size);
		return;
	}
	if(!room_were_in)
	{
		qDebug("Got rematch request, but not in a room");
		return;
	}
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
	if(!boarddispatch)
	{
		qDebug("Couldn't get board dispatch on room were in: %d", room_were_in);
		return;
	}
	boarddispatch->recvRematchRequest();
	
	/* FIXME, we'll need to send the request and get the accept and
	* see what comes next... */
	//their id, our id, two more bytes
#ifdef RE_DEBUG
	printf("rematch request: 69c3: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}

//0x6ec3:
void TygemConnection::handleRematchAccept(unsigned char * msg, unsigned int size)
{
	bool were_black;
	if(size != 6)
	{
		qDebug("rematch accept packet with strange size: %d", size);
		return;
	}
	/* FIXME, we'll need to send the request and get the accept and
	 * see what comes next... */
	//their id, our id, two more bytes
#ifdef RE_DEBUG
	printf("rematch request accept: 6ec3: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	//610a a804 128f
	//we need to check our_player_id against first two bytes
	if(!room_were_in)
	{
		qDebug("Not in a room for rematch accept");
		return;
	}
	BoardDispatch * boarddispatch = getIfBoardDispatch(room_were_in);
	if(!boarddispatch)
	{
		qDebug("handle rematch accept has no board dispatch");
		return;
	}
	MatchRequest * m = new MatchRequest();
	GameData * gd = boarddispatch->getGameData();
	m->number = room_were_in;
	m->last_game_code = gd->game_code;
	m->rematch = true;
	m->board_size = gd->board_size;
	m->komi = gd->komi;
	m->handicap = gd->handicap;
	m->free_rated = gd->free_rated;
	if(m->handicap == 0 && gd->komi > 0.0)
	{
		//switch colors?
		//yes switch colors, we send the match offer..
		if(gd->black_name == getUsername())
			were_black = false;
		else
			were_black = true;
	}
	else
		were_black = (gd->black_name == getUsername());
	m->opponent_id = gd->opponent_id;
	m->opponent_is_challenger = false;
	if(were_black)
	{	
		m->opponent = gd->white_name;
		m->their_rank = gd->white_rank;
		m->our_name = gd->black_name;
		m->our_rank = gd->black_rank;
		m->color_request = MatchRequest::BLACK;
		m->challenger_is_black = false;	//?
	}
	else
	{
		m->opponent = gd->black_name;
		m->their_rank = gd->black_rank;
		m->our_name = gd->white_name;
		m->our_rank = gd->white_rank;
		m->color_request = MatchRequest::WHITE;
		m->challenger_is_black = false;
	}
	m->timeSystem = gd->timeSystem;
	m->maintime = gd->maintime;
	m->periodtime = gd->periodtime;
	m->stones_periods = gd->stones_periods;
	NetworkConnection::closeBoardDispatch(room_were_in);	//doesn't close window necessarily
	
	
	delete m;
	
	//BoardDispatch * boarddispatch = getBoardDispatch(room_were_in);		//should open new game
	//this needs also to send something else to trigger whatever
}

/* This is also used during game negotiation, not just rematches */
//0x327d
void TygemConnection::handleMatchDecline(unsigned char * msg, unsigned int size)
{
	PlayerListing * player;
	unsigned char * p = msg;
#ifdef RE_DEBUG
	printf("327d size: %d\n", size);
#endif //RE_DEBUG
	if(p[0] + (p[1] << 8) != our_player_id)
	{
		qDebug("*** Woah, 327d rematch decline not meant for us!");
		return;
	}
	p += 2;
	player = getDefaultRoomDispatch()->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("Can't find player for id %02x%02x for match decline\n", p[0], p[1]);
		return;
	}
	p += 2;
	// those last two bytes?
#ifdef RE_DEBUG
	printf("0x327d: unexplained bytes: %02x %02x\n", p[0], p[1]);
#endif //RE_DEBUG
	GameDialogDispatch * gameDialogDispatch = getIfGameDialogDispatch(*player);
	if(!gameDialogDispatch)
	{
		qDebug("Got 327d but we don't have a game dialog for %s", player->name.toLatin1().constData());
		return;
	}
	gameDialogDispatch->recvRefuseMatch(GD_REFUSE_DECLINE);
	
	/* FIXME, we need to add something here for rematch declines */
	
#ifdef RE_DEBUG
	printf("0x327d: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}
//b0b3
/*void TygemConnection::handleResign(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	GameData * gr;
	unsigned char * p = msg;
	PlayerListing * player;
	if(size != 8)
	{
		qDebug("Resign message of strange size: %d", size);
		return;
	}
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("Can't find player for id %02x%02x for resign\n", p[0], p[1]);
		return;
	}
	p += 2;
	unsigned short game_code = p[0] + (p[1] << 8);
	unsigned short game_number = game_code_to_number[game_code];
	boarddispatch = getIfBoardDispatch(game_number);
	if(boarddispatch)
	{
		GameResult aGameResult;
		gr = boarddispatch->getGameData();
		if(!gr)
		{
			printf("Can't get game record for resign msg\n");
			return;
		}
		aGameResult.result = GameResult::RESIGN;
		if(gr->black_name == player->name)
		{
			aGameResult.winner_color = stoneWhite;
			aGameResult.winner_name = gr->black_name;
			aGameResult.loser_name = gr->white_name;  //necessary FIXME???
		}
		else
		{
			aGameResult.winner_color = stoneBlack;
			aGameResult.winner_name = gr->white_name;
			aGameResult.loser_name = gr->black_name;
		}
		boarddispatch->recvResult(&aGameResult);
		if(game_number == our_game_being_played)
			killActiveMatchTimers();
	}
}*/

//c9b30c00 c801 ea0c 000000f1
void TygemConnection::handleEnterScoring(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	PlayerListing * player;
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
#ifdef RE_DEBUG
	printf("c9b3 size: %d\n", size);
#endif //RE_DEBUG
	if(!player)
	{
		printf("Can't find player for id %02x%02x for enterscoremode\n", p[0], p[1]);
		return;
	}
	p += 2;
	unsigned short game_code = p[0] + (p[1] << 8);
	boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(boarddispatch)
	{
#ifdef RE_DEBUG
		qDebug("Entering scoring mode");
#endif //RE_DEBUG
		boarddispatch->recvEnterScoreMode();
		if(game_code_to_number[game_code] == our_game_being_played)
		{
			deadStonesList.clear();
			receivedOppDone = false;
			killActiveMatchTimers();
		}
	}
}

void TygemConnection::killActiveMatchTimers(void)
{
	qDebug("killing timers");
	if(matchRequestKeepAliveTimerID)
	{
		killTimer(matchRequestKeepAliveTimerID);
		matchRequestKeepAliveTimerID = 0;
	}
	if(matchKeepAliveTimerID)
	{
		killTimer(matchKeepAliveTimerID);
		matchKeepAliveTimerID = 0;
	}
}

void TygemConnection::startMatchTimers(bool ourTurn)
{
	if(ourTurn)
	{
		qDebug("Starting keep alive timer");
		matchKeepAliveTimerID = startTimer(39000);
		matchRequestKeepAliveTimerID = 0;
	}
	else
	{
		qDebug("Starting request keep alive timer");
		matchRequestKeepAliveTimerID = startTimer(39000);
		matchKeepAliveTimerID = 0;
	}
}

/* Final f1b31400 b006 640b 2f05 5c22 0102 1106 0804 1106
 * is basically the same thing... plus a couple more bytes...
 * makes me think we should write only one handler or something.
 * or maybe e7b31400 2f05 640b 2f05 5c6d 0102 1106 0804 1106
 * is really the final ?!?!? FIXME 
 * Basically we need to watch this very carefully, to see
 * clicks and unlicks, or even observe a game that we play. */
//e2b31200 2f05 640b 2f05 2863 0201 1106 0804
void TygemConnection::handleRemoveStones(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	int i;
	unsigned char * p = msg;
	bool unremove;
	int number_of_marks;
	PlayerListing * player;
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("Can't find player for id %02x%02x for enterscoremode\n", p[0], p[1]);
		return;
	}
	p += 2;
	unsigned short game_code = p[0] + (p[1] << 8);
	boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		printf("Can't get board dispatch for remove stones for %02x%02x\n", p[0], p[1]);
		return;
	}
	p += 2;
	//another player id
	p += 2;
#ifdef RE_DEBUG
	printf("e2b3: ");
	for(i = 0; i < (int)size - 6; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	//weird two bytes
	p += 2;
	// a 01 or 02
	unremove = (p[0] == 2);
	p++;
	number_of_marks = p[0];
	p++;
	MoveRecord aMove;
	aMove.x = p[0];
	aMove.y = p[1];
	/* First one is the new one, the rest is the full list */
	/* Also, I think if its an unremove, its removed from the list,
	 * both in this packet and ensuing ones */
	
	if(unremove)
	{
		aMove.flags = MoveRecord::UNREMOVE_AREA;
		if(our_game_being_played == game_code_to_number[game_code])
		{
			std::vector <MoveRecord>::iterator it;
			for(it = deadStonesList.begin(); it != deadStonesList.end(); it++)
			{
				if(it->x == aMove.x &&
				    it->y == aMove.y)
				{
					qDebug("removing %d %d from dead stones list", aMove.x, aMove.y);
					deadStonesList.erase(it);
					break;
				}
			}
		}
	}
	else
	{
		aMove.flags = MoveRecord::REMOVE_AREA;
		if(our_game_being_played == game_code_to_number[game_code])
		{
		qDebug("pushing %d %d onto dead stones list\n", aMove.x, aMove.y);
			deadStonesList.push_back(aMove);
		}
	}
	
	boarddispatch->recvMove(&aMove);
	/* What about mark undos??? FIXME */
	p += 2; 	//skip the rest
	/*
		while(number_of_marks--)
		{
			MoveRecord aMove;
			aMove.flags = MoveRecord::REMOVE;
			aMove.x = p[0];
			aMove.y = p[1];
			p += 2;
			boarddispatch->recvMove(&aMove);
		}
	*/
}


//f1b3
/* This is definitely like an opponent has hit done kind of
 * msg, not that that means anything to us until we hit done */
void TygemConnection::handleStonesDone(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	int i;
	unsigned char * p = msg;
	int number_of_marks;
	PlayerListing * player;
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("Can't find player for id %02x%02x for enterscoremode\n", p[0], p[1]);
		return;
	}
	p += 2;
	unsigned short game_code = p[0] + (p[1] << 8);
	unsigned short game_number = game_code_to_number[game_code];
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		printf("Can't get board dispatch for remove stones for %02x%02x\n", p[0], p[1]);
		return;
	}
	p += 2;
	//another player id
	p += 2;
#ifdef RE_DEBUG
	printf("0xf1b3: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	//weird two bytes
	if(game_number == our_game_being_played)
	{
		done_response = p[0] + (p[1] << 8);
		receivedOppDone = true;
	}
	p += 2;
	// a 01 or 02
	p++;
	number_of_marks = p[0];
	p++;
	/* First one is the new one, the rest is the full list */
	MoveRecord aMove;
	aMove.flags = MoveRecord::REMOVE;
	aMove.x = p[0];
	aMove.y = p[1];
	boarddispatch->recvKibitz(QString(), QString("%1 has hit done...").arg(player->name));
	//boarddispatch->recvMove(&aMove);
	/* What about mark undos??? FIXME */
	p += 2; 	//skip the rest
	
	/* FIXME, we might actually want to just clear and readd all the stones
	 * listed.  I mean otherwise we're sort of negating the point of
	 * the done message */
	/*
	while(number_of_marks--)
	{
	MoveRecord aMove;
	aMove.flags = MoveRecord::REMOVE;
	aMove.x = p[0];
	aMove.y = p[1];
	p += 2;
	boarddispatch->recvMove(&aMove);
}
	*/
}

//0672
void TygemConnection::handleGameResult(unsigned char * msg, unsigned int size)
{
	BoardDispatch * boarddispatch;
	int i;
	unsigned char * p = msg;
	unsigned short game_number = (p[0] << 8) + p[1];
	bool white_wins;
	int white_seconds, white_periods, black_seconds, black_periods;
	int c_seconds, o_seconds, c_periods, o_periods;
	char c_flags, o_flags;
	//this is sent if we join an ended game as well
	//must be result where 0671 is match opened
	//0670 is sent if, for instance, resign
	//while we're watching it, but not after over
	
	//but 0670 can be sent without 0672 apparently
	//during game, but again, we would get 0672 alone
	//if it was after game
	//lastly, looks like 0672 might happen without SUR?
	//maybe only when adjourned or something?
	//but we definitely need to handle it and we might
	//want to have it send result
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("Can't get board for game number %d", game_number);
		return;
	}
	GameData * gd = boarddispatch->getGameData();
	p += 2;
	white_wins = p[0] ^ gd->white_first_flag;
	p += 2;
	//winner name
	p += 14;
	//winner rank
	p += 2;
	//0300 0000 0115 0000
	p += 8;
	//times
	bool blacktimefirst = true;
	c_periods = p[0];
	c_seconds = p[1] + (p[2] * 60);
	c_flags = p[3];
	p += 4;
	//second four are other player
	o_periods = p[0];
	o_seconds = p[1] + (p[2] * 60);
	o_flags = p[3];
	p += 4;
	// likely times are backwards or oriented by loser/winner
	if(gd->white_first_flag)
	{
		black_seconds = c_seconds;
		white_seconds = o_seconds;
		if(c_flags & 0x20)
			black_periods = c_periods;
		else
			black_periods = -1;
		if(o_flags & 0x20)
			white_periods = o_periods;
		else
			white_periods = -1;
	}
	else
	{
		black_seconds = o_seconds;
		white_seconds = c_seconds;
		if(c_flags & 0x20)
			white_periods = c_periods;
		else
			white_periods = -1;
		if(o_flags & 0x20)
			black_periods = o_periods;
		else
			black_periods = -1;
	}
	
	/* We should most likely kill the timer then...
	 * no, really setGamePhase in setResult should fix that */
	boarddispatch->recvTime(TimeRecord(white_seconds, white_periods), TimeRecord(black_seconds, black_periods));
	
	
	//sent after surrender:
	//0003 0101 c4ba d3ea b3bf d4c6 0000 0000
	//0000 0014 0300 0000 0115 0000 0318 00a0
	//021e 0060		
	//winner color then name probably
	//0002 0101 6164 3838 0000 0000 0000 0000 
	//0000 0016 0100 002d 0147 0000 0312 0980
	//032d 0740 02d0 0616
	
	//00e0 0001 cef2 b5bd 0000 0000 0000 0000 
	//0000 0018 0100 0055 0124 0000 (wt)0214 00a0
	//(bt)0114 0060 01b8 0616
	
	//time loss
	//0002 0001 7365 7237 3031 3133 3000 0000
	//0000 0012 0700 0000 00f7 0000 (wt)0000 00a0
	//(bt)031e 0060
	
	//time loss, time loss name first
	//0003 0001 7065 7465 7269 7573 0000 0000
	//0000 0008 0900 0000 0000 0000 0100 0580
	//0000 0080

	if(boarddispatch)
		boarddispatch->recvKibitz(0, "0x672 " + QString::number(white_wins));
	printf("***** 0x0672: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
}

//0x0683:	//clock stop? //enter score?
void TygemConnection::handleScoreMsg1(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	BoardDispatch * boarddispatch;
	int i;
	//0087 aa01 0101 0000 031e 00a0 0334 0040
	printf("0x0683: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	boarddispatch = getIfBoardDispatch((msg[0] << 8) + msg[1]);
	if(boarddispatch)
		boarddispatch->recvKibitz(0, "0x683");
	//after this 7b
	//0087 0101 6261 696e 6974 6500 0000 0000 
	//0000 0011 6261 696e 6974 6500 0000 0002
	//00c5 0000 0000 0000
	//0087 aa01 0300 0000 031e 00a0 0334 0040
	
	//another 7b 83 pair:
	//0087 0001 baac d0a6 bba8 0000 0000 0000
	//0000 0011 6861 6e78 6961 6f68 7561 0002
	//00c5 0000 0000 0000
	
	//0087 aa01 0201 0000 031e 00a0 0334 0040
	
	// and two more pairs, same game
	//0x067b: 
	//00870001baacd0a6bba80000000000000000001168616e7869616f687561000201c6000000000000
	//0087aa0104010100031e00a003340040

	//008701016261696e6974650000000000000000116261696e697465000000000201c6000000000000
	
	//0087aa0105000100031e00a003340040
	//followed by 0672
	//0087 0101 6261 696e 6974 6500 0000 0000
	//0000 0011 0000 0055 0116 0000 031e 00a0
	//0334 0040 0024 0668
}

//an REM -1 -1 likely proceeds these as meaning done?
//this is intrusion to peterius after marking all of his black stones dead
//002c067b
//0002 0101 7065746572697573000000000000000865746572697573000000000200c5000000000000
//0x0683: 0002aa0103000000012a0480012c0440
//067b  black keeps hitting confirm confirm c7 versus c5 is likely score?
//0010 0101 7065746572697573000000000000000865746572697573000000000200c7000000000000
//0x067b:   
void TygemConnection::handleScoreMsg2(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	BoardDispatch * boarddispatch;
	int i;
	
	//I"m thinking actually draw request
	//no I think this is a request, count maybe?  or is that 6a?
	//maybe winner message?, unlikely, several sent and 0201c5 as well below
	//00020101616438380000000000000000000000
	//1661643838000000000000000200c5000000000000
	
	//we saw alternating 067b and 0683, with the 83 starting the sequence
	//had something to do with hitting done or agree, in the end black
	//(I think black) just resigned
	//01c50101c4f4d2bbb5b6000000000000000000124e6965596944616f0000000200c7000000000000
	//01c5 aa01 0600 0000 030b 00a0 031e 0060
	//01c50001caa4c1fa3200000000000000000000127368656e676c6f6e6732000201c9000000000000
	//and then right before DSC move message:

	//0x067b: 01c50101c4f4d2bbb5b6000000000000000000124e6965596944616f0000000201c6000000000000
	//0x0683: 01c5 aa01 0401 0100 030b 00a0 031e 0060
	//0x067b: 01c50001caa4c1fa3200000000000000000000127368656e676c6f6e6732000200c6000000000000
	//0x0683: 01c5 aa01 0500 0000 030b 00a0 031e 0060
	boarddispatch = getIfBoardDispatch((msg[0] << 8) + msg[1]);
	if(boarddispatch)
		boarddispatch->recvKibitz(0, "0x67b");
	printf("***** 0x067b: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	// probably related to hitting done
	//008701016261696e6974650000000000000000116261696e697465000000000200c5000000000000
}

//e7b3
void TygemConnection::handleScore(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	int i;
	unsigned char * p = msg;
	unsigned short game_id, game_code;
	int number_of_marks;
	PlayerListing * player;
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("Can't find player for id %02x%02x for enterscoremode\n", p[0], p[1]);
		return;
	}
	p += 2;
	game_code = p[0] + (p[1] << 8);
	game_id = game_code_to_number[game_code];
	boarddispatch = getIfBoardDispatch(game_id);
	if(!boarddispatch)
	{
		printf("Can't get board dispatch for remove stones for %02x%02x\n", p[0], p[1]);
		return;
	}
	p += 2;
	//another player id
	p += 2;
#ifdef RE_DEBUG
	printf("e7b3: ");
	for(i = 0; i < (int)size - 6; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	//weird two bytes
	p += 2;
	// a 01 or 02
	p++;
	number_of_marks = p[0];
	p++;
	/* First one is the new one, the rest is the full list */
	MoveRecord aMove;
	aMove.flags = MoveRecord::REMOVE;
	aMove.x = p[0];
	aMove.y = p[1];
	//boarddispatch->recvMove(&aMove);
	/* What about mark undos??? FIXME */
	p += 2; 	//skip the rest
	/*
	while(number_of_marks--)
	{
	MoveRecord aMove;
	aMove.flags = MoveRecord::REMOVE;
	aMove.x = p[0];
	aMove.y = p[1];
	p += 2;
	boarddispatch->recvMove(&aMove);
}
	*/
	boarddispatch->recvResult(0);
	
	//double check, this appropriate here? FIXME, we need a func for it
	GameListing * aGameListing = roomdispatch->getGameListing(game_id);
	if(aGameListing)
	{
		aGameListing->running = false;
		roomdispatch->recvGameListing(aGameListing);
		game_code_to_number.erase(game_id);
		our_game_being_played = 0;
		/* Can't imagine how but... */
		clearRoomsWithoutGames(game_id);
	}
}

/* This is really ugly, but we have to try everything to fix this bug */
void TygemConnection::clearRoomsWithoutGames(unsigned short game_id)
{
	return;
	std::vector<GameListing *>::iterator pl_i;
	for(pl_i = rooms_without_games.begin(); pl_i != rooms_without_games.end(); pl_i++)
	{
		if((*pl_i)->number == game_id)
		{
			rooms_without_games.erase(pl_i);
			break;
		}
	}	
}

//f6b3
void TygemConnection::handleTimeLoss(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	PlayerListing * player;
	unsigned short game_code, game_number;
#ifdef RE_DEBUG
	printf("f6b3 size: %d\n", size);	
#endif //RE_DEBUG
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		qDebug("No player for %02x %02x for time loss!!\n", p[0], p[1]);
		return;
	}
	p += 2;
	game_code = p[0] + (p[1] << 8);
	game_number = game_code_to_number[game_code];
	
	
	
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("No board dispatch for %02x%02x", p[0], p[1]);
		return;
	}
	p += 2;
	GameData * aGameData = boarddispatch->getGameData();
	
	GameResult aGameResult;
	aGameResult.result = GameResult::TIME;
	aGameResult.game_number = game_number;
	aGameResult.loser_name = player->name;
	if(aGameData->white_name == player->name)
	{
		//winner and loser colors??!?!? FIXME FIXME
		aGameResult.winner_name = aGameData->black_name;
		aGameResult.winner_color = stoneBlack;
	}
	else
	{
		aGameResult.winner_name = aGameData->white_name;
		aGameResult.winner_color = stoneWhite;
	}
	boarddispatch->recvResult(&aGameResult);
	/* Maybe the last 4 bytes are the remaining 1 second of time, maybe
	 * its color or something?*/
#ifdef RE_DEBUG
	printf("f6b3: %02x%02x%02x%02x\n", p[0], p[1], p[2], p[3]);
#endif //RE_DEBUG
	/* What about killing the listing or something??*/
}
//these could be time messages
//00 10 06 7D 01 8D 01 01 03 18 00 60 03 07 00 A0
//00 10 06 7D 01 8D 01 01 03 17 00 60 03 07 00 A0
//00 10 06 7D 01 8D 01 01 03 16 00 60 03 07 00 A0
//00 10 06 7D 01 8D 01 01 03 15 00 60 03 07 00 A0  
//00 10 06 7D 01 8D 00 01 03 1E 00 A0 03 11 00 60 
//00 10 06 7D 01 8D 00 01 03 1D 00 A0 03 11 00 60
//00 10 06 7D 01 8D 00 01 03 16 00 A0 03 11 00 60
void TygemConnection::handleTime(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	GameData * gr;
	PlayerListing * player;
	int i;
	unsigned char c_periods, o_periods;
	unsigned char c_flags, o_flags;
	unsigned int c_seconds, o_seconds;
	bool inPeriodTime;
	int white_seconds, white_periods;
	int black_seconds, black_periods;
	unsigned short game_number = (p[0] << 8) + p[1];
	bool player_is_black;
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("No board dispatch for %02x%02x", p[0], p[1]);
		return;
	}
	gr = boarddispatch->getGameData();
	if(!gr)
	{
		qDebug("Can't get game record for board: %d", game_number);
		return;
	}
	printf("TIME: ");
	for(i = 0; i < size; i++)
		printf("%02x", p[i]);
	printf("\n");
	//000f0101032b0b4003100480  blacks time first
	//000f00010311048003300b40  whites time first
	
	//000f 0100 d0e9 bfd 5beb 2000000000000000000
	//146875616e67736869313100020000000300bf000000ff000
	//000000000

	//00020001031f0c40032c0d80	//black
	//00020101032a0d80031d0c40	//white
	
	//0002 0100 6164 3838 0000000000000000000000
	//166164383800696a69750000020000000500aa000000ff000
	//		000000000

	p += 2;
	// this gets reversed
	player_is_black = !p[0] ^ gr->white_first_flag;
	p += 2;
	//first four are this current players turn
	c_periods = p[0];
	c_seconds = p[1] + (p[2] * 60);
	c_flags = p[3];
	p += 4;
	//second four are other player
	o_periods = p[0];
	o_seconds = p[1] + (p[2] * 60);
	o_flags = p[3];
	if(player_is_black)
	{
		black_seconds = c_seconds;
		white_seconds = o_seconds;
		if(c_flags & 0x20)
			black_periods = c_periods;
		else
			black_periods = -1;
		if(o_flags & 0x20)
			white_periods = o_periods;
		else
			white_periods = -1;
	}
	else
	{
		black_seconds = o_seconds;
		white_seconds = c_seconds;
		if(c_flags & 0x20)
			white_periods = c_periods;
		else
			white_periods = -1;
		if(o_flags & 0x20)
			black_periods = o_periods;
		else
			black_periods = -1;
	}
	/* If its our match and our turn, we need to block this possibly... */
	
	/* We should most likely kill the timer then... */
	boarddispatch->recvTime(TimeRecord(white_seconds, white_periods), TimeRecord(black_seconds, black_periods));
	
	/*if(player_is_black)
		boarddispatch->recvTime(TimeRecord(0, 0), TimeRecord(black_seconds, black_periods));
	else
		boarddispatch->recvTime(TimeRecord(white_seconds, white_periods), TimeRecord(0, 0));
	*/
}

/* FIXME disturbing implication.  When we were sending the moves one off,
 * it would draw them on the place we clicked, but then draw the last move
 * on the place we got back.  This indicates inconsistencies in move
 * placement code, serious issue */
//0668
void TygemConnection::handleMove(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	GameData * gr;
	PlayerListing * player;
	int i;
	int periods;
	bool inPeriodTime;
	unsigned short seconds;
	unsigned short player_id;
	unsigned short game_number = (p[0] << 8) + p[1];
	int player_number, player_number2, number0;
	//unsigned short game_number = game_code_to_number[game_id];
	//00 24 06 68 01 8D 01 01 FF FF FF FF 00 01  R`.$.h..........
	//AF 00 53 54 4F 20 30 20 32 20 31 20 31 35 20 33  ..STO 0 2 1 15 3
	//20 0A 00 00 00 00
	
	//00 24 06 68 01 8D 00 01 FF FF   ......$.h......
	//FF FF 00 02 A3 00 53 54 4F 20 30 20 33 20 32 20  ......STO 0 3 2 
	//33 20 31 35 20 0A 00 00 00 00
	
	//00 24 06 68 01 8D 01 01 FF FF FF FF 00 4B BD 00  .$.h.........K..
	//53 54 4F 20 30 20 37 36 20 31 20 31 31 20 39 20  STO 0 76 1 11 9 
	//0A 00 00 00 
	//00 24 06 68 01 8D 00 01 FF FF FF FF 00 4C B2 00  .$.h.........L..
	//53 54 4F 20 30 20 37 37 20 32 20 39 20 31 33 20  STO 0 77 2 9 13 
	//0A 00 00 00  
	//white resigns
	//00 20 06 68 01 8D 00 01 FF FF FF FF 00 B3 D1 01  . .h............
	//53 55 52 20 30 20 31 38 31 20 32 20 0A 00 00 00  SUR 0 181 2 ....
	
	previous_opponent_move_pass = false;
	/*if(size != 32)
	{
		qDebug("Move of strange size: %d", size);
		return;
	}*/
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("No board dispatch for %02x%02x", p[0], p[1]);
		return;
	}
	gr = boarddispatch->getGameData();
	if(!gr)
	{
		//FIXME
		qDebug("Shouldn't be a board dispatch here\n");
		return;
	}
	p += 2;
	//redundant but make sure
	if(p[0] == 0x00)
		player_number2 = 0;
	else if(p[0] == 0x01)//white
		player_number2 = 1;
	else
		qDebug("Strange color byte in move");
	p += 2;
	p += 4;		//ffff ffff
	p += 4;		//could be time
	//STO 0 move_number player_number x y 0a
	//SUR 0 move_number player_number 0a
	//REM 0 312 -1 -1 0 1 
	//REM 0 307 5 6 1 0 
	//REM 0 306 10 14 2 0 			un remove?
	//REM 0 number x y player_number 0
	//DSC 1 0 1			//possibly disagreement?
	//DSC 1 0 2 
	//SKI 0 player_number		//skip? pass?
	
	//BAC 0 246 1 	//part of review
	//CSP 0 233 0
	//CST 0 234 
	//FOR 0 186 1
	printf("%d %s\n", player_number2, (char *)p);
	if(strncmp((char *)p, "STO ", 4) == 0)
	{
		MoveRecord * aMove = new MoveRecord();
		p += 4;
		if(sscanf((char *)p, "%d %d %d %d %d", &number0, &aMove->number, &player_number, &aMove->x, &aMove->y) != 5)
		{
			qDebug("Bad Move");
			delete aMove;
			return;
		}
		move_message_number = aMove->number;
		aMove->number--;	//first move is... handicap?
		aMove->x++;
		aMove->y++;
		if(player_number == 1)
			aMove->color = stoneBlack;
		else if(player_number == 2)
			aMove->color = stoneWhite;
		else
			qDebug("Strange player number: %d", player_number);
		if(number0 != 0)
			qDebug("Number0 = %d\n", number0);
		//if(aMove->x == 0 || aMove->y == 0)	//doubt it FIXME
		//	aMove->flags = MoveRecord::PASS;
		boarddispatch->recvMove(aMove);
		delete aMove;
	} 
	else if(strncmp((char *)p, "SKI ", 4) == 0)
	{
		p += 4;
		//int move_number;
		if(sscanf((char *)p, "%d %d %d", &number0, &player_number) != 2)
		{
			qDebug("Bad skip message");
			return;
		}
		//FIXME move number
		previous_opponent_move_pass = true;
		boarddispatch->recvKibitz(QString(), "SKI");
	} 
	else if(strncmp((char *)p, "SUR ", 4) == 0)
	{
		p += 4;
		int move_number;
		if(sscanf((char *)p, "%d %d %d", &number0, &move_number, &player_number) != 3)
		{
			qDebug("Bad resign message");
			return;
		}
		GameResult aGameResult;
		aGameResult.result = GameResult::RESIGN;
		if(player_number == 1)
		{
			aGameResult.winner_color = stoneWhite;
			aGameResult.winner_name = gr->black_name;
			aGameResult.loser_name = gr->white_name;  //necessary FIXME???
		}
		else
		{
			aGameResult.winner_color = stoneBlack;
			aGameResult.winner_name = gr->white_name;
			aGameResult.loser_name = gr->black_name;
		}
		boarddispatch->recvResult(&aGameResult);
#ifdef FIXME
		if(game_number == our_game_being_played)
			killActiveMatchTimers();
#endif //FIXME
	} 
	else if(strncmp((char *)p, "REM ", 4) == 0)
	{
		MoveRecord * aMove = new MoveRecord();
		/* FIXME, fix potential bug with unremoving as on cyberoro ?!?? */
		aMove->flags = MoveRecord::REMOVE_AREA;
		int remove_flag;
		p += 4;
		if(sscanf((char *)p, "%d %d %d %d %d", &number0, &aMove->number, &aMove->x, &aMove->y, &remove_flag) != 5)
		{
			qDebug("Bad Move");
			delete aMove;
			return;
		}
		move_message_number = aMove->number;
		if(aMove->x == -1 && aMove->y == -1)
		{
			boarddispatch->recvKibitz(QString(), "REM -1 -1");
			//done scoring! recvKibitz, etc.
			//seems to be only one of these
			//no, I see two
			delete aMove;
			return;
		}
		aMove->number--;
		aMove->x++;
		aMove->y++;
		/*if(player_number == 1)
			aMove->color = stoneBlack;
		else if(player_number == 2)
			aMove->color = stoneWhite;
		else
			qDebug("Strange player number: %d", player_number);*/
		if(number0 != 0)
			qDebug("Number0 = %d\n", number0);
		//if(aMove->x == 0 || aMove->y == 0)	//doubt it FIXME
		//	aMove->flags = MoveRecord::PASS;
		boarddispatch->recvMove(aMove);
		delete aMove;
	}
	else if(strncmp((char *)p, "DSC ", 4) == 0)
	{
		MoveRecord * aMove = new MoveRecord();
		/* FIXME, fix potential bug with unremoving as on cyberoro ?!?? */
		aMove->flags = MoveRecord::REMOVE_AREA;
		int number2;
		p += 4;
		if(sscanf((char *)p, "%d %d %d", &number0, &number2, &player_number) != 3)
		{
			qDebug("Bad Move");
			delete aMove;
			return;
		}
		if(aMove->x == -1 && aMove->y == -1)
		{
			boarddispatch->recvKibitz(QString(), "DSC " + QString::number(number0) + " " + QString::number(number2) + " " + QString::number(player_number));
			//done scoring! recvKibitz, etc.
			//seems to be only one of these
			//no, I see two
			delete aMove;
			return;
		}
		aMove->number--;
		aMove->x++;
		aMove->y++;
		/*if(player_number == 1)
		aMove->color = stoneBlack;
		else if(player_number == 2)
		aMove->color = stoneWhite;
		else
		qDebug("Strange player number: %d", player_number);*/
		if(number0 != 0)
			qDebug("Number0 = %d\n", number0);
		//if(aMove->x == 0 || aMove->y == 0)	//doubt it FIXME
		//	aMove->flags = MoveRecord::PASS;
		boarddispatch->recvMove(aMove);
		delete aMove;
	}
	else
		printf("Unknown: %s\n", (char *)p);
}

void TygemConnection::handlePass(unsigned char * msg, unsigned int size, int pass_number)
{
	unsigned char * p = msg;
	BoardDispatch * boarddispatch;
	int i;
	int game_number = (p[0] << 8) + p[1];
	p += 2;
	bool white;
	//white = !p[0];		//maybe first player?
	p += 2;
	unsigned char name[11];
	QString encoded_name, ascii_name;
	
	strncpy((char *)name, (char *)p, 11);
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	//0669 0002 0101 7065 7465 7269 7573 0000 
	//0000 0000 0008 6574 6572 6975 7300 0000
	//0002 0000 0000 0000 0000

	p += 14;
	
	//rank byte
	p += 2;
	strncpy((char *)name, (char *)p, 11);
	ascii_name = QString((char *)name);
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("Can't get board for %d for pass\n", game_number);
		return;
	}
	p += 10;
	p++;
	white = (p[0] != 0x02);
	//there's an 02 much later here and that must be... black color?
	/* Do we do this here or on the handleMove SKI? FIXME */
	MoveRecord * move = new MoveRecord();
	move->flags = MoveRecord::PASS;
	move->color = white ? stoneWhite : stoneBlack;

	//boarddispatch->recvKibitz(0, QString(encoded_name) + " passes " + QString::number(move->color));
	boarddispatch->recvKibitz(0, QString(white ? "White" : "Black") + " passes " + QString::number(move->color));
	boarddispatch->recvMove(move);
	//black
	//00870101 6261 696e 6974 6500 0000 0000 0000 00116261696e69746500000000020000000000000000
	printf("**** 0x0669: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
}
//e6af
void TygemConnection::handleUndo(unsigned char * msg, unsigned int size)
{
	int i;
	unsigned char * p = msg;
	unsigned short player_id, game_code;
	
	if(size != 10)
	{
		qDebug("undo of strange size: %d", size);
		return;
	}
	
	player_id = p[0] + (p[1] << 8);
	game_code = p[2] + (p[3] << 8);
	
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("no board dispatch for undo");
		return;
	}
	PlayerListing * player = getDefaultRoomDispatch()->getPlayerListing(player_id);
	if(!player)
	{
		qDebug("No player listing for our supposed opponent");
		return;
	}
	p += 4;
	p += 2;	//color byte
	MoveRecord * move = new MoveRecord();
	move->flags = MoveRecord::REQUESTUNDO;
	move->number = p[0] + (p[1] << 8);
	
	//boarddispatch->recvKibitz(0, player->name + QString(" wants undo to move %1.").arg(opp_requests_undo_move_number));
	
	boarddispatch->recvMove(move);
	delete move;
	//from sender perspective	encode(e)
	//e6af 0e00 b50d 4c02 01 00 0a 00 00 00
#ifdef RE_DEBUG
	printf("0xe6af undo: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	
}

//0xf0af
void TygemConnection::handleDeclineUndo(unsigned char * msg, unsigned int size)
{
	unsigned short player_id, game_code;
	unsigned char * p = msg;
	
	if(size != 10)
	{
		qDebug("declineundo of strange size: %d", size);
		return;
	}
	player_id = p[0] + (p[1] << 8);
	game_code = p[2] + (p[3] << 8);
	
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("no board dispatch for decline undo");
		return;
	}
	PlayerListing * player = getDefaultRoomDispatch()->getPlayerListing(player_id);
	if(!player)
	{
		qDebug("No player listing for our supposed opponent");
		return;
	}
	
	boarddispatch->recvKibitz(0, player->name + " refused undo.");
	
	//4a06 e40b 0100 0500 001d
#ifdef RE_DEBUG
	printf("0xf0af decline undo: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}

void TygemConnection::handleAcceptUndo(unsigned char * msg, unsigned int size)
{
	unsigned short player_id, game_code;
	unsigned char * p = msg;
	
	player_id = p[0] + (p[1] << 8);
	game_code = p[2] + (p[3] << 8);
	
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("no board dispatch for decline undo");
		return;
	}
	PlayerListing * player = getDefaultRoomDispatch()->getPlayerListing(player_id);
	if(!player)
	{
		qDebug("No player listing for our supposed opponent");
		return;
	}
	boarddispatch->recvKibitz(0, player->name + " accepted undo.");
	
	p += 4;
	p += 2;	//color byte
	MoveRecord * move = new MoveRecord();
	move->flags = MoveRecord::UNDO;
	move->number = p[0] + (p[1] << 8);
	
	//boarddispatch->recvKibitz(0, player->name + QString(" wants undo to move %1.").arg(opp_requests_undo_move_number));
	
	boarddispatch->recvMove(move);
	delete move;
	
	//4a06 e40b 01000500001d
#ifdef RE_DEBUG
	printf("0xebaf accept undo: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
}


//0x42f4: 4d75 bd00 0101 ea00 0d08 0200 0101 000b
//0x42f4: 4d75 bd00 0101 ec00 0b02 0300 0101 000b
//0x42f4: 4d75 bd00 0000 ed00 0201 0f00 0101 000b

//same as 4cf4, 4cf4 being for in room?
//d7af
void TygemConnection::handleMoveList(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	BoardDispatch * boarddispatch;
	int number_of_moves;
	int i;
	bool pass = false;
	bool enterScore = false;
	GameData * gr;
	MoveRecord * aMove = new MoveRecord();
	p += 2;
	unsigned short game_code = p[0] + (p[1] << 8);
	p += 2;
	boarddispatch = getIfBoardDispatch(game_code_to_number[game_code]);
	if(!boarddispatch)
	{
		qDebug("No board dispatch for %02x%02x", p[0], p[1]);
		return;
	}
	gr = boarddispatch->getGameData();
	/* FIXME This is rather unfortunate as well but until we fix
	 * qgo_network code's move_counter nonsense, a hold over
	 * from the old code that I just copied without completely
	 * replacing, we do need to take this into account. FIXME */
	
	number_of_moves = p[0] + (p[1] << 8);
	p += 2;
	p += 4;
#ifdef RE_DEBUG
	printf("Move List!!!!\n");
#endif //RE_DEBUG
	//player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	aMove->flags = MoveRecord::NONE;
	//for(i = (gr->handicap ? 1 : 0) ; i < number_of_moves + (gr->handicap ? 1 : 0); i++)
	/* We're sending these starting with 1 because moves after
	 * we'll be offset in that way. i.e., ORO starts with 1*/
	if(gr->handicap)
		aMove->color = stoneWhite;
	else
		aMove->color = stoneBlack;
	for(i = 1; i < number_of_moves + 1; i++)
	{
		//aMove->color = stoneNone;	//keep resetting this
		aMove->number = i;	//or i + !?
		aMove->x = p[0];
		aMove->y = p[1];
#ifdef RE_DEBUG
		printf("Move: %d %d\n", p[0], p[1]);
#endif //RE_DEBUG
		if(aMove->x == 0)
		{
			aMove->flags = MoveRecord::PASS;
			if(pass/* && aMove->color == stoneWhite*/)	//white passes last
				enterScore = true;
			pass = true;
		}
		else
			pass = false;
		p += 2;
		boarddispatch->recvMove(aMove);
		if(aMove->x == 0)
			aMove->flags = MoveRecord::NONE;	//for next iteration
		if(aMove->color == stoneWhite)
			aMove->color = stoneBlack;
		else
			aMove->color = stoneWhite;
	}
	delete aMove;
	if(p != (msg + size))
		qDebug("Move list with strange size: %d", size);
	if(enterScore)
		boarddispatch->recvEnterScoreMode();
}

//4cf4 4 zeroes different FIXME join
/* I think this is like for dead games or records or something.  broadcast at the least
* Broadcast is extremely likely.  May also start with that 38f4 betting message */
void TygemConnection::handleMoveList2(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	unsigned char * p = msg;
	BoardDispatch * boarddispatch;
	int number_of_moves;
	int i;
	unsigned short game_id;
	MoveRecord * aMove = new MoveRecord();
	
	p += 2;
	game_id = p[0] + (p[1] << 8);
	
	GameListing * listing = roomdispatch->getGameListing(connecting_to_game_number);
	if(!listing)
	{
		qDebug("Can't get listing for game number %d", connecting_to_game_number);
		closeBoardDispatch(connecting_to_game_number);
		return;	
	}
	listing->game_code = game_id;
	game_code_to_number[listing->game_code] = listing->number;
	boarddispatch = getBoardDispatch(listing->number);
	
	roomdispatch->recvGameListing(listing);	//okay?
	

	connecting_to_game_number = 0;
	
	p += 2;
	
	
	number_of_moves = p[0] + (p[1] << 8);
	p += 2;
	//p += 4;		//doesn't have this !!!
	p += 2;			//instead
#ifdef RE_DEBUG
	printf("Move List Broadcast/Special %d %d!!!!\n", listing->number, listing->game_code);
#endif //RE_DEBUG
	//player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	aMove->flags = MoveRecord::NONE;
	//for(i = (aGameData->handicap ? 1 : 0) ; i < number_of_moves + (aGameData->handicap ? 1 : 0); i++)
	for(i = 1; i < number_of_moves + 1; i++)
	{
		
		aMove->color = stoneNone;	//keep resetting this
		aMove->number = i;	//or i + !?
		aMove->x = p[0];
		aMove->y = p[1];
#ifdef RE_DEBUG
		printf("Move: %d %d\n", p[0], p[1]);
#endif //RE_DEBUG
		if(aMove->x == 0)
			aMove->flags = MoveRecord::PASS;
		p += 2;
		boarddispatch->recvMove(aMove);
		if(aMove->x == 0)
			aMove->flags = MoveRecord::NONE;	//for next iteration
	}
	delete aMove;
	if(p != (msg + size))
		qDebug("MoveList2 strange size: %d", size);
}

// note that we receive response on this, so likely we should
// set ourself, check box there FIXME
//9a65 maybe?!?!? not a result... probably invitation allowed!!!
void TygemConnection::handleInvitationSettings(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	int i;
	unsigned char * p = msg;
	PlayerListing * player;
#ifdef RE_DEBUG
	printf("0x9a65: ");
#endif //RE_DEBUG
	
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		printf("no player for %02x%02x\n", p[0], p[1]);
		return;
	}
#ifdef RE_DEBUG
	printf("%s ", player->name.toLatin1().constData());
	for(i = 2; i < (int)size; i++)
		printf("%02x", p[i]);
	printf("\n");
#endif //RE_DEBUG
	p += 2;
	//3rd byte after name is invitation settings
	player->info = getStatusFromCode(p[2], player->rank);
	
}

QString TygemConnection::getStatusFromCode(unsigned char code, QString rank)
{
	return "";
}

int TygemConnection::compareRanks(QString rankA, QString rankB)
{
	if(rankA.contains("p") && !rankB.contains("p"))
		return 1;
	if(rankB.contains("p") && !rankA.contains("p"))
		return -1;
	if(rankA.contains("d") && !rankB.contains("d"))
		return 1;
	if(rankB.contains("d") && !rankA.contains("d"))
		return -1;
	if(rankA.contains("k"))
	{
		QString buffer = rankA;
		buffer.replace(QRegExp("[pdk]"), "");
		int ordinalA = buffer.toInt();
		buffer = rankB;
		buffer.replace(QRegExp("[pdk]"), "");
		int ordinalB = buffer.toInt();
		if(ordinalA > ordinalB)
			return -1;
		else if(ordinalA == ordinalB)
			return 0;
		else
			return 1;
	}
	else
	{
		QString buffer = rankA;
		buffer.replace(QRegExp("[pdk]"), "");
		int ordinalA = buffer.toInt();
		buffer = rankB;
		buffer.replace(QRegExp("[pdk]"), "");
		int ordinalB = buffer.toInt();
		if(ordinalA > ordinalB)
			return 1;
		else if(ordinalA == ordinalB)
			return 0;
		else
			return -1;
	}
}


//00 34 06 39 01 8D 01 00 E5 D0 D2 A3 C6 E5 C4 DA  .4.9............
//CD E2 00 00 00 00 00 1A 78 69 61 6F 79 61 6F 38  ........xiaoyao8
//38 00 00 02 00 00 00 B2 00 A0 00 00 00 FF 00 00  8...............
//00 00 00 00  
//0639
void TygemConnection::handleBoardOpened(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	PlayerListing * player;
	PlayerListing * playerA, * playerB;
	//GameListing * game;
	GameData * aGameData;
	unsigned short player_id;
	unsigned short game_id;
	unsigned short game_number;
	int handicap;
	unsigned short black_seconds, white_seconds;
	int black_periods, white_periods;
	int i;
	float komi;
	bool white_in_byoyomi, black_in_byoyomi;
	bool we_are_challenger = false;
	int board_size;
	bool nigiri = false;
	game_number = (p[0] << 8) + p[1];
	p += 2;
	//this doesn't look like a match message
	
	//001f010070657465726975730000000000000008706574657269757300000002000000010000000000ff000000000000
#ifdef RE_DEBUG
	printf("0639 match: \n");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG

	boarddispatch = getBoardDispatch(game_number);
	/* We have to pass the game data record on the 0639
	 * because the boardwindow is created by the dispatch
	 * upon recv a record.  The board window is necessary
	 * to receive the observers from 0666, 0667 illicited
	 * by the 0665 message, the 0665 message also illicits
	 * the 0671 info message.  So this is the way it must
	 * be done */
	aGameData = boarddispatch->getGameData();
	aGameData->number = game_number;
	if(playing_game_number == game_number)
		aGameData->gameMode = modeMatch;
	/*GameListing * gl = roomdispatch->getGameListing(game_number);
	//FIXME get first time records
	if(gl)
	{
		aGameData->black_name = gl->black_name();
		aGameData->black_rank = gl->black_rank();
		aGameData->white_name = gl->white_name();
		aGameData->white_rank = gl->white_rank();
		// Unfortunately relevant to time records
		aGameData->white_first_flag = gl->white_first_flag;
	}*/
	boarddispatch->openBoard();
	
	//observing_game_count++;	//maybe we could put this in 0671 FIXME?
	
	/* Moved here from 0639 because 0671 probably
	 * clears list or something on recv first data 
	 * and then back to 0639 because apparently,
	 * it is the observer request that illicits 0671?*/
	if(playing_game_number != game_number)
		sendObserversRequest(game_number);
	return;
}


//0671
void TygemConnection::handleMatchOpened(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	PlayerListing * player;
	PlayerListing * playerA, * playerB;
	PlayerListing * opponent = 0;
	GameData * aGameData;
	unsigned short player_id;
	unsigned short game_id;
	unsigned short game_number;
	unsigned char name[11];
	QString encoded_nameA, encoded_nameB;
	QString rankA, rankB;
	int handicap;
	unsigned short black_seconds, white_seconds;
	int black_periods, white_periods;
	int i;
	float komi;
	bool white_in_byoyomi, black_in_byoyomi;
	bool we_are_challenger = false;
	int board_size;
	bool nigiri = false;
	game_number = (p[0] << 8) + p[1];
	p += 2;
	
	boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("No board dispatch for %d", game_number);
		return;
	}
	aGameData = boarddispatch->getGameData();
	
	aGameData->number = game_number;
	
	/* Don't want to use gl for names, but maybe need it for
	 * white first flag?  Hope not */
	GameListing * gl = roomdispatch->getGameListing(game_number);
	//FIXME get first time records
	if(gl)
	{
		//aGameData->black_name = gl->black_name();
		//aGameData->black_rank = gl->black_rank();
		//aGameData->white_name = gl->white_name();
		//aGameData->white_rank = gl->white_rank();
		/* Unfortunately relevant to time records */
		aGameData->white_first_flag = gl->white_first_flag;
	}
#ifdef RE_DEBUG
	printf("0671 match: \n");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	for(i = 0; i < (int)size; i++)
		printf("%c", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	// we should see if there's time info in here or the
			// one we're currently using.  we need to initially set time
			// we also seem to get this one when rematch started
	
	/* FIXME If this is our match, we should
	* most likely do bd->startGame(); here */
			
	//0087 0001 baac d0a6 bba8 0000 0000 0000
	//0000 0011 6261 696e 6974 6500 0000 0000
	//0000 0011 012c 1e03 0000 0000 0100 0002
	//ffff ffff ffff ffff 0002 0002 6861 6e78
	//6961 6f68 7561 0002 6261 696e 6974 6500
	//0000 0002 48f6 56cd
	//definitely thinking game info
	//00050001c0cfcdb7000000000000000000000014ced2d0cdced2d0e30000000000000014
	//012c1e030000010001000002ffffffffffffffff000100044a61636b44616e6e7900000267647368
	//6a0000000000000248f6a64e

	//second option high minutes
	//0003000170657465726975730000000000000008696e74727573696f6e00000000000009
	//012c28010100010001000002ffffff
	//ffffffffff00010001706574657269757300000002696e74727573696f6e00000248fcebe0
	
	//probably players turn 0001
	p += 2;
	//name and rank
	strncpy((char *)name, (char *)p, 11);
	encoded_nameA = serverCodec->toUnicode((char *)name, strlen((char *)name));
	p += 14;
	p++;
	//rank byte
	if(p[0] < 0x12)
		rankA = QString::number(0x12 - p[0]) + 'k';
	else if(p[0] > 0x1a)
		rankA = QString::number(p[0] - 0x1a) + 'p';
	else
		rankA = QString::number(p[0] - 0x11) + 'd';
	p++;
	//name and rank		
	strncpy((char *)name, (char *)p, 11);
	encoded_nameB = serverCodec->toUnicode((char *)name, strlen((char *)name));
	p += 14;
	p++;
	//rank byte
	if(p[0] < 0x12)
		rankB = QString::number(0x12 - p[0]) + 'k';
	else if(p[0] > 0x1a)
		rankB = QString::number(p[0] - 0x1a) + 'p';
	else
		rankB = QString::number(p[0] - 0x11) + 'd';
	p++;
	//time settings
	//type seconds minutes periods
	/*switch(p[0])
	{
		case 0x01:
			aGameData->timeSystem = byoyomi;
			break;
		default:
			printf("Time system: %02x!!!\n", p[0]);
			break;
	}*/
	// TIME is wrong FIXME
	aGameData->maintime = (p[0] << 8) + p[1];
	aGameData->periodtime = p[2];
	aGameData->stones_periods = p[3];
	printf("0671 TIME SETTINGS: %d %d %d %d %d %d\n", p[0], p[1], p[2], p[3], p[4], p[5]);
	aGameData->handicap = p[4];
	//FIXME we need to safely set handicap
	p += 4;
	p += 2;
	aGameData->white_first_flag = p[0];
	p += 2;
	if(gl && gl->white_first_flag != aGameData->white_first_flag)
		qDebug("gl and 0671 white first flag differ !!!!!");
	//000a000170657465726975730000000000000008696e74727573696f6e00000000000009
	//04b0 1e03 0100 0000 0100 0002 ffff ffff ffff ffff 000d 0001
	//7065 7465 7269 7573 0000 0001 696e 7472 7573 696f 6e00 0002 48fd 0098
	
	//0005000170657465726975730000000000000008696e74727573696f6e000000000000090e101e030100000001000002ffffff
	//ffffffffff000f0001706574657269757300000001696e74727573696f6e00000248fd050a
	
	//challenger, second name is black
	//0004000170657465726975730000000000000008696e74727573696f6e000000000000090e101e030100000001000002ffffff
	//ffffffffff00100001706574657269757300000001696e74727573696f6e00000248fd0ce1

	p += 4;
	p += 4;
	p += 4;
	//this is white/black or our picture code: p[1]
	p += 2;
	//possibly this is black or their picture code: p[1]
	p += 2;
	//these are probably the ascii names but we'll skip for now FIXME
	p += 10;
	//p[1] is color byte
	/* We assume ascii names are in same order */
	//if(p[1] == 0x02)
	boarddispatch->recvKibitz(QString(), "0671 wff " + QString::number(aGameData->white_first_flag)
				+ " gl " + (gl ? QString::number(gl->white_first_flag) : "NA") +	
				+ " " + QString::number(p[1], 16) + " " + QString::number(p[13], 16));
	if(!aGameData->white_first_flag)
	{
		aGameData->white_name = encoded_nameA;
		aGameData->white_rank = rankA;
		qDebug("white is %s", encoded_nameA.toLatin1().constData());
	}
	//else if(p[1] == 0x01)
	else
	{
		aGameData->black_name = encoded_nameA;
		aGameData->black_rank = rankA;
		qDebug("black is %s", encoded_nameA.toLatin1().constData());
	}
	//else
	//	qDebug("Strange color byte %d, line: %d", p[1], __LINE__);
	p += 2;
	p += 10;
	if(playing_game_number == game_number)
		opponent = roomdispatch->getPlayerListing(encoded_nameB);
	//p[1] is color byte
	//if(p[1] == 0x02)
	if(aGameData->white_first_flag)
	{
		aGameData->white_name = encoded_nameB;
		aGameData->white_rank = rankB;
		qDebug("white is %s", encoded_nameB.toLatin1().constData());
	}
	//else if(p[1] == 0x01)
	else
	{
		aGameData->black_name = encoded_nameB;
		aGameData->black_rank = rankB;
		qDebug("black is %s", encoded_nameB.toLatin1().constData());
	}
	//else
	//	qDebug("Strange color byte %d, line: %d", p[1], __LINE__);
	p += 2;
	boarddispatch->gameDataChanged();
	if(playing_game_number == game_number)
	{
		if(!opponent)
			qDebug("Can't get opponent to close game dialog");
		else
			MatchRequest * mr = getAndCloseGameDialogDispatch(*opponent);
		//FIXME time
		boarddispatch->recvTime(TimeRecord(aGameData->maintime, -1), TimeRecord(aGameData->maintime, -1));
		boarddispatch->startGame();	//starts timers
		boarddispatch->swapColors(true);
	}
	//observing_game_count++;
	previous_opponent_move_pass = false;
	move_message_number = 0;
	
	return;
}

void TygemConnection::handleObserveAfterJoining(unsigned char * msg, unsigned int size)
{
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	//BoardDispatch * boarddispatch;
	unsigned char * p = msg;
	PlayerListing * player;
	/*GameListing * game;
	GameData * aGameData;
	unsigned short game_id;
	int handicap;
	unsigned short black_seconds, white_seconds;
	int black_periods, white_periods;
	unsigned short period_time;
	float komi;
	bool send_handicap_move = false;
	bool white_in_byoyomi, black_in_byoyomi;*/
	
	player = roomdispatch->getPlayerListing(p[0] + (p[1] << 8));
#ifdef FIXME
	if(player)
		printf("Currently %s's turn...\n", player->name.toLatin1().constData());
	else
		printf("Can't find player for for d2af\n");
	printf("38f4\n");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //FIXME
	//FIXME
}

//join with 0646 as well
//0645
void TygemConnection::handleMatchOffer(unsigned char * msg, unsigned int size, MIVersion version)
{
	unsigned char * p = msg;
	//0645 
	//6269 7473 0000 0000 0000 0000 0000 0014
	//c4ba d3ea b3bf d4c6 0000 0000 0000 0014
	//0003 012c 1e03 0000 0000 001e 0000 0000
	//0000 0000 0000 0000 0000 0100 6269 7473
	//0000 0000 0000 0002 6a65 7070 3100 0000
	//0000 0002
			
	//as we recv this, we are peterius, first name
	//7065 7465 7269 7573 0000 0000 0000 0008
	//696e 7472 7573 696f 6e00 0000 0000 0009
	//0038 012c 2801 0100 0100 0002 0000 0000
	//0000 0000 0000 0000 0000 0000 0000 0000
	//0000 0000 0000 0000 696e 7472 7573 696f
	//6e00 0002
	int i;
	unsigned short game_number;
	unsigned char name[11];
	QString encoded_name;
	PlayerListing * opponent;
	
	printf("0x0645/6 match offer: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	strncpy((char *)name, (char *)p, 11);
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	if(encoded_name != getUsername())
		return;
	if(version == acknowledge)
		return;		//whatever... like TCP is lossy
	p += 14;
	//00 rank
	p += 2;
	strncpy((char *)name, (char *)p, 11);
	encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	opponent = getDefaultRoomDispatch()->getPlayerListing(encoded_name);
	if(!opponent)
	{
		qDebug("Match offer from unknown opponent");
		return;
	}
	p += 14;
	//00 rank
	p += 2;
	game_number = (p[0] << 8) + p[1];
	p += 2;
	//time settings
	MatchRequest * tempmr = new MatchRequest();
	tempmr->number = game_number;
	tempmr->opponent = encoded_name;
	tempmr->timeSystem = byoyomi;
	tempmr->maintime = (p[0] << 8) + p[1];
	tempmr->periodtime = p[2];
	tempmr->stones_periods = p[3];
	tempmr->handicap = p[4];
	//0001
	switch(p[6])
	{
		case 0x00:
			tempmr->color_request = MatchRequest::WHITE;
			break;
		case 0x01:
			tempmr->color_request = MatchRequest::BLACK;
			break;
		case 0x02:
			tempmr->color_request = MatchRequest::NIGIRI;
			break;
		default:
			printf("Strange byte in match offer: %02x", p[6]);
			break;
	}
	
	//0004 0e10 1e03 0100
	if(version == offer)
	{	
		//apparently acknowledge no longer necessary?
		//sendMatchOffer(*tempmr, acknowledge);
		
		//Here, we actually want to pop up game dialog
		GameDialogDispatch * gameDialogDispatch = getGameDialogDispatch(*opponent);
		gameDialogDispatch->recvRequest(tempmr, getGameDialogFlags()); 	
	}
	else if(version == accept)
	{
		//match offer accept
		//00580646
		//0x0646: 70657465726975730000000000000008696e74727573696f6e000000000000090032012c2801010001
		//000001ffffffff000000000001000000000000706574657269757300000002696e74727573696f6e000002
		//00a00616
		MatchRequest * mr = getAndCloseGameDialogDispatch(*opponent);
		if(!mr)
		{
			qDebug("Can't get match request for opponent");
			//return;
		}
		//FIXME, maybe should be mr here, but tempmr from filled out
		//above is probably better
		sendStartGame(*tempmr);
	}
	delete tempmr;
	return;
}

//5fc3
void TygemConnection::handleResumeMatch(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	int i;
	unsigned short room_number = p[0] + (p[1] << 8);
	p += 2;
	//our id
	//their id
	// ffs? lots of ffs?
#ifdef RE_DEBUG
	printf("resume adjourned? 5fc3?: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	//resume adjourned? 5fc3?: 5a00 8d02 b50d ffffffffffffffffffffffff
	//1a81 message: 6402 {1f 02008d02b50dcd059c023a0b5d00bd030f08} game of lostsoul 8d
	//	02 and giantcat b50d
	
	/* It only get set as our game if its got our name in it
	 * and even that may not be enough but... treat like
	 * 0a7d from here.
	
	 * Actually, this may be a bad idea because it can screw up
	 * the player names !!! ours is always first regardless of color
	 * we'd have to make sure that 0a7d is not relied on for color
	 * and more than that that the d2af does set the color up properly.*/
	/* More than that, it causes NewRoom to complain about size 18 */
	handleNewRoom(msg, size);
#ifdef RE_DEBUG
	qDebug("Resuming\n");
#endif //RE_DEBUG
}
/* FIXME, combine these very common handler messages into one function */

//b5b3
void TygemConnection::handleAdjournRequest(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	//unsigned short opponent_id = p[0] + (p[1] << 8);
	unsigned short game_code = p[2] + (p[3] << 8);
	unsigned short game_number = game_code_to_number[game_code];
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("Received adjourn decline on nonexistent board");
		return;
	}
	boarddispatch->recvRequestAdjourn();

	p += 2;
	//their id game id, maybe color?
#ifdef RE_DEBUG
	qDebug("b5b3 size: %d", size);
#endif //RE_DEBUG
}

//bfb3
void TygemConnection::handleAdjournDecline(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	//unsigned short opponent_id = p[0] + (p[1] << 8);
	unsigned short game_code = p[2] + (p[3] << 8);
	unsigned short game_number = game_code_to_number[game_code];
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("Received adjourn decline on nonexistent board");
		return;
	}
	boarddispatch->recvRefuseAdjourn();

	p += 2;
	//their id game id, maybe color?
//5304 430d 0100 0071
#ifdef RE_DEBUG
	printf("0xbfb3 likely adjourn decline: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");

	qDebug("bfb3 size: %d", size);
#endif //RE_DEBUG
}

//bab3
void TygemConnection::handleAdjourn(unsigned char * msg, unsigned int size)
{
	unsigned char * p = msg;
	//unsigned short opponent_id = p[0] + (p[1] << 8);
	unsigned short game_code = p[2] + (p[3] << 8);
	unsigned short game_number = game_code_to_number[game_code];
	BoardDispatch * boarddispatch = getIfBoardDispatch(game_number);
	if(!boarddispatch)
	{
		qDebug("Received adjourn  on nonexistent board");
		return;
	}
	/* FIXME, why do we need to close the board dispatch, shouldn't
	* the adjournGame do that?  or should it, maybe we can talk after? */
	boarddispatch->adjournGame();
	//connection->closeBoardDispatch(memory);
#ifdef RE_DEBUG
	qDebug("bab3 size: %d", size);
#endif //RE_DEBUG
}

//0643
void TygemConnection::handleMatchInvite(unsigned char * msg, unsigned int size)
{
	//7065 7465 7269 7573 0000 0000 0000 0001
	//7065 7465 7269 7573 0000 0000 696e 7472
	//7573 696f 6e00 0000 0000 0009 696e 7472
	//7573 696f 6e00 0002 0100 ffff
	
	//696e74727573696f6e00000000000000000000000000000000000000706574657269757300000000000000087
	//0657465726975730000000202003000

	unsigned char * p = msg;
	unsigned char name[11];
	unsigned short room_number;
	GameListing * gl;
	if(size != 60)
	{
		qDebug("MatchInvite of size %d\n", size);
		// probably should print it out and exit FIXME
	}
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	p += 14;
	invite_byte = p[1];
	p += 2;
	p += 12;
	/* Can't we make a special thing to do all these repetitive string copies? */
	strncpy((char *)name, (char *)p, 11);
	QString encoded_name = QString((char *)name);
	//QString encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
			
	
	PlayerListing * player = roomdispatch->getPlayerListing(encoded_name);
	if(!player)
	{
		qDebug("Match invite from unknown player");
		return;
	}
	p += 26;
	p++;
	opponent_is_challenger = (bool)(p[0] - 1);
	p++;
	p += 2;		//possible first, second player settings
	room_number = p[0] + (p[1] << 8);
	if(room_number != 0xffff)
	{
		// this is created room
		playing_game_number = room_number;
		sendObserve(room_number);
		//this also asks for an observer list request which may
		//FIXME not be necessary if its our match, don't think
		//they send it, we're first ones in
		sendJoin(room_number);
		return;
	}
	MatchInviteDialog * mid = new MatchInviteDialog(player->name, player->rank);
	int mid_return = mid->exec();
	
	if(mid_return == 1)
		sendMatchInvite(*player, accept);
	else if(mid_return == -1)
		sendMatchInvite(*player, decline);
}

//this is match invite response, auto refuse
//0644: 
//0644 7065 7465 7269 7573 0000 0000 0000
//0008 0000 0000 0000 0000 0000 0000 696e
//7472 7573 696f 6e00 0000 0000 0001 696e
//7472 7573 696f 6e00 0002 010b ffff
void TygemConnection::handleMatchInviteResponse(unsigned char * msg, unsigned int size)
{
	//7065 7465 7269 7573 0000 0000 0000 0001
	//7065 7465 7269 7573 0000 0000 696e 7472
	//7573 696f 6e00 0000 0000 0009 696e 7472
	//7573 696f 6e00 0002 0100 ffff
			
	unsigned char * p = msg;
	unsigned char name[11];
	if(size != 60)
	{
		qDebug("MatchInvite of size %d\n", size);
		// probably should print it out and exit FIXME
	}
	//actually maybe our offer is still screwed up FIXME
	printf("*** 0x0644: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	RoomDispatch * roomdispatch = getDefaultRoomDispatch();
	p += 28;
	/* Can't we make a special thing to do all these repetitive string copies? */
	strncpy((char *)name, (char *)p, 11);
	QString encoded_name = QString((char *)name);
	//QString encoded_name = serverCodec->toUnicode((char *)name, strlen((char *)name));
	
	PlayerListing * player = roomdispatch->getPlayerListing(encoded_name);
	if(!player)
	{
		qDebug("Match invite from unknown player");
		return;
	}
	p += 28;
	p++;
	//this byte seems to determine accept versus decline, etc.
	//0x0b is autodecline, 0x00 is offer or decline here, 0x01
	switch(p[0])
	{
		case 0x00:
		{
			QString text = player->name + " has declined invitation";
			QMessageBox mb(tr("Invite declined"), text, QMessageBox::Information, QMessageBox::Ok | QMessageBox::Default,
				       QMessageBox::NoButton, QMessageBox::NoButton);
			mb.exec();
		}
			break;
		case 0x01:
			player_accepted_match = player;
			sendCreateRoom();
			break;
		case 0x0b:
		{
			/* FIXME, I'm thinking 0b may mean that they're already in a game
			 * in addition? */
			QString text = player->name + " is not accepting invitations";
			QMessageBox mb(tr("Invite declined"), text, QMessageBox::Information, QMessageBox::Ok | QMessageBox::Default,
				       QMessageBox::NoButton, QMessageBox::NoButton);
			mb.exec();
		}
			break;
	}
}

//0637
void TygemConnection::handleCreateRoomResponse(unsigned char * msg, unsigned int size)
{
	unsigned short room_number = (msg[0] << 8) + msg[1];
	int i;
	
	if(size != 12)	//4?
	{
		qDebug("create room response of size: %d", size);
		//return;
	}
	/* FIXME We're going to assume for now that we're getting
	 * this because we received a match response accept and
	 * so we'll just send the join message and pray for rain */
	if(msg[2] != 0x01 || msg[3] != 0xff)
	{
		printf("roomcreate: %02x %02x\n", msg[2], msg[3]);
	}
	//0x0637 room create?: 0129 01ff 0000 0000 0000 0000
	printf("0x0637 room create?: ");
	for(i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
	
	sendJoin(room_number);
	//join should also put us in room with board?
	sendMatchInvite(*player_accepted_match, create, room_number);
	//not sure of timing of this
	
	//when we offer and its accepted and we create, we don't
	//get a starting message, I think we're supposed to open the
	//board on receipt of 0637 maybe, 0665 send is strange, don't
	//know what that's for, no response anyway, that we know of
	
	/* Try creating board here FIXME with MatchOpened 0639*/
	BoardDispatch * boarddispatch = getBoardDispatch(room_number);
	GameData * aGameData = boarddispatch->getGameData();
	aGameData->number = room_number;
	aGameData->gameMode = modeMatch;
	/*GameListing * gl = roomdispatch->getGameListing(game_number);
	//FIXME get first time records
	if(gl)
	{
		aGameData->black_name = gl->black_name();
		aGameData->black_rank = gl->black_rank();
		aGameData->white_name = gl->white_name();
		aGameData->white_rank = gl->white_rank();
		// Unfortunately relevant to time records
		//aGameData->white_first_flag = gl->white_first_flag;
	}*/
	boarddispatch->openBoard();
	playing_game_number = room_number;	//needed for observer check send
}

//0x1e7d
void TygemConnection::handleMatchRoomOpen(unsigned char * msg, unsigned int size)
{
	
#ifdef RE_DEBUG
	printf("0x1e7d: ");
	for(int i = 0; i < (int)size; i++)
		printf("%02x", msg[i]);
	printf("\n");
#endif //RE_DEBUG
	
	unsigned char * p = msg;
	if(size != 6)
	{
		qDebug("MatchRoomOpen of size %d\n", size);
		// probably should print it out and exit FIXME
	}
	if(p[0] + (p[1] << 8) != our_player_id)
	{
		qDebug("Received MatchRoomOpen for someone besides us");
		return;
	}
	p += 2;
	PlayerListing * player = getDefaultRoomDispatch()->getPlayerListing(p[0] + (p[1] << 8));
	if(!player)
	{
		qDebug("MatchInviteAccept from unknown player");
		return;
	}
	p += 2;
	// no idea what this is
	//setRoomNumber(p[0] + (p[1] << 8));
	
	GameDialogDispatch * gd = getGameDialogDispatch(*player);
	gd->recvRequest(0, getGameDialogFlags());
}

/* The point of this is that we should clear out the observer lists
 * on rooms we leave, etc. */
void TygemConnection::setRoomNumber(unsigned short number)
{
	if(room_were_in)
	{
		BoardDispatch * bd = getIfBoardDispatch(room_were_in);
		if(bd)
			bd->clearObservers();
	}
	room_were_in = number;
}

/* There's also FIXME FIXME FIXME, 
 * these massive 0x8129 messages at the start of the game, 
 * looks like an opponents records or something... maybe a picture?
 * I have no idea. and a 0x7c29 right before it, short but who knows? 
 * And then after an 0x8629, again, short. check "truegame" and "truegame2".
 * Actually wait, I think these are coming from after a game!! like result
 * messages. */

/* These needs to be more versatile later */
void TygemConnection::onReady(void)
{
	//sendInvitationSettings(true);	//for now
	qDebug("Ready!\n");
}


/* Because the IGS protocol is garbage, we have to break encapsulation here
 * and in BoardDispatch Wow FIXME, don't think we need this.*/
BoardDispatch * TygemConnection::getBoardFromAttrib(QString black_player, unsigned int black_captures, float black_komi, QString white_player, unsigned int white_captures, float white_komi)
{
	BoardDispatch * board;
	std::map<unsigned int, class BoardDispatch *>::iterator i;
	std::map<unsigned int, class BoardDispatch *> * boardDispatchMap =
		boardDispatchRegistry->getRegistryStorage();
	for(i = boardDispatchMap->begin(); i != boardDispatchMap->end(); i++)
	{
		board = i->second;
		if(board->isAttribBoard(black_player, black_captures, black_komi, white_player, white_captures, white_komi))
			return board;
	}
	return NULL;
}

BoardDispatch * TygemConnection::getBoardFromOurOpponent(QString opponent)
{
	BoardDispatch * board;
	std::map<unsigned int, class BoardDispatch *>::iterator i;
	std::map<unsigned int, class BoardDispatch *> * boardDispatchMap =
		boardDispatchRegistry->getRegistryStorage();
	/* Parser may supply our name from the IGS protocol... this is ugly
	 * but I'm really just trying to reconcile what I think a real
 	 * protocol would be like with the IGS protocol */
	if(opponent == username)
		opponent = "";
	for(i = boardDispatchMap->begin(); i != boardDispatchMap->end(); i++)
	{
		board = i->second;
		if(board->isOpponentBoard(username, opponent))
			return board;
	}
	return NULL;
}

void TygemConnection::requestGameInfo(unsigned int /*game_id*/)
{
	//FIXME disable the button
}

void TygemConnection::requestGameStats(unsigned int /*game_id*/)
{
	//FIXME disable the button
}

GameData * TygemConnection::getGameData(unsigned int game_id)
{
	/* FIXME, this is weird, I mean I know the caller usually
	 * checks that boarddispatch is solid but... */
	/* I don't think we use this ?!?!? */
	qDebug("getGameData on COC deprecated");
	return getBoardDispatch(game_id)->getGameData();
}

int TygemConnection::time_to_seconds(const QString & time)
{
	QRegExp re = QRegExp("([0-9]{1,2}):([0-9]{1,2})");
	int min, sec;
	
	if(re.indexIn(time) >= 0)
	{
		min = re.cap(1).toInt();
		sec = re.cap(2).toInt();	
	}
	else
		qDebug("Bad time string");
	
	return (60 * min) + sec;
}