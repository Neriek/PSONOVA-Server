/****************************************************************/
/*	Author:	Sodaboy                                             */
/*  Modified by: gatchi (github.com/gatchi/PSONOVA-Server)      */
/*	Date:	07/22/2008                                          */
/*	accountadd.c :  Adds an account to the Tethealla PSO        */
/*			server.                                             */
/*                                                              */
/****************************************************************/

//#define NO_SQL

#ifdef _WIN32
#include	<windows.h>
#endif

#ifdef __gnu_linux__
#include	<stdlib.h>
#endif

#include	<stdio.h>
#include	<string.h>
#include	<time.h>

#ifndef NO_SQL
#include	<mysql.h>
#endif
#include	<md5.h>

/* Computes the message digest for string inString.
   Prints out message digest, a space, the string (in quotes) and a
   carriage return.
 */
void MDString (inString, outString)
char *inString;
char *outString;
{
  unsigned char c;
  MD5_CTX mdContext;
  unsigned int len = strlen (inString);

  MD5Init (&mdContext);
  MD5Update (&mdContext, inString, len);
  MD5Final (&mdContext);
  for (c=0;c<16;c++)
  {
	  *outString = mdContext.digest[c];
	  outString++;
  }
}

#ifdef NO_SQL

#define MAX_ACCOUNTS 2000

typedef struct st_account {
	char username[18];
	char password[33];
	char email[255];
	unsigned regtime;
	char lastip[16];
	long long lasthwinfo;
	unsigned guildcard;
	int isgm;
	int isbanned;
	int islogged;
	int isactive;
	int teamid;
	int privlevel;
	unsigned char lastchar[24];
} L_ACCOUNT_DATA;

L_ACCOUNT_DATA *account_data[MAX_ACCOUNTS];

unsigned ds,ds2,num_accounts;
unsigned highid = 0;
int ds_found, new_record, free_record;

#endif

void UpdateDataFile ( const char* filename, unsigned count, void* data, unsigned record_size, int new_record )
{
	FILE* fp;
	unsigned fs;

	fp = fopen (filename, "r+b");
	if (fp)
	{
		fseek (fp, 0, SEEK_END);
		fs = ftell (fp);
		if ((count * record_size) <= fs)
		{
			fseek (fp, count * record_size, SEEK_SET);
			fwrite (data, 1, record_size, fp);
		}
		else
			printf ("Could not seek to record for updating in %s\n", filename);
		fclose (fp);
	}
	else
	{
		fp = fopen (filename, "wb");
		if (fp)
		{
			fwrite (data, 1, record_size, fp); // Has to be the first record...
			fclose (fp);
		}
		else
			printf ("Could not open %s for writing!\n", filename);
	}
}

void LoadDataFile ( const char* filename, unsigned* count, void** data, unsigned record_size )
{
	FILE* fp;
	unsigned ch;

	printf ("Loading \"%s\" ... ", filename);
	fp = fopen (filename, "rb");
	if (fp)
	{
		fseek (fp, 0, SEEK_END);
		*count = ftell (fp) / record_size;
		fseek (fp, 0, SEEK_SET);
		for (ch=0;ch<*count;ch++)
		{
			data[ch] = malloc (record_size);
			if (!data[ch])
			{
				printf ("Out of memory!\nHit [ENTER]");
				exit (1);
			}
			fread (data[ch], 1, record_size, fp);
		}
		fclose (fp);
	}
	printf ("done!\n");
}

int
main( int argc, char * argv[] )
{
	char inputstr[255] = {0};
	char username[17];
	char password[34];
	char password_check[17];
	char md5password[34] = {0};
	char email[255];
	char email_check[255];
	unsigned char ch;
	time_t regtime;
	unsigned reg_seconds;
	unsigned char max_fields;

#ifndef NO_SQL
	MYSQL * myData;
	char myQuery[255] = {0};
	MYSQL_ROW myRow ;
	MYSQL_RES * myResult;
#endif
	int num_rows, pw_ok, pw_same;
	unsigned guildcard_number;

	char mySQL_Host[255] = {0};
	char mySQL_Username[255] = {0};
	char mySQL_Password[255] = {0};
	char mySQL_Database[255] = {0};
	unsigned int mySQL_Port;
	int config_index = 0;
	char config_data[255] = {0};

	unsigned char MDBuffer[17] = {0};

	FILE* fp;

	if ( ( fp = fopen ("tethealla.ini", "r" ) ) == NULL )
	{
		printf ("The configuration file tethealla.ini appears to be missing.\n");
		return 1;
	}
	
	while ((fgets (config_data, 255, fp) != NULL) && (config_index < 5))
	{
		if (config_data[0] != 0x23)
		{
			if (config_index < 0x04)
			{
				// Remove newlines and any carriage returns
				char len = strlen (&config_data[0]);
				if (config_data[len-1] == '\n')
				{
					config_data[len-1]  = '\0';
					if (config_data[len-2] == '\r')
						config_data[len-2] = '\0';
				}
			}
			switch (config_index)
			{
			case 0x00:
				// MySQL Host
				strcpy (&mySQL_Host[0], &config_data[0]);
				break;
			case 0x01:
				// MySQL Username
				strcpy (&mySQL_Username[0], &config_data[0]);
				break;
			case 0x02:
				// MySQL Password
				strcpy (&mySQL_Password[0], &config_data[0]);
				break;
			case 0x03:
				// MySQL Database
				strcpy (&mySQL_Database[0], &config_data[0]);
				break;
			case 0x04:
				// MySQL Port
				mySQL_Port = atoi (&config_data[0]);
				break;
			default:
				break;
			}
			config_index++;
		}
		
		// Clear config_data after each fgets
		memset (config_data, 0, 255 * sizeof (char));
	}
	fclose (fp);
	
#ifdef NO_SQL
	num_accounts = 0;
	LoadDataFile ("account.dat", &num_accounts, &account_data[0], sizeof(L_ACCOUNT_DATA));
#endif
#ifndef NO_SQL
	if ( (myData = mysql_init((MYSQL*) 0)) && mysql_real_connect( myData, &mySQL_Host[0], &mySQL_Username[0], &mySQL_Password[0], NULL, mySQL_Port, NULL, 0 ) )
	{
		if ( mysql_select_db( myData, mySQL_Database ) < 0 )
		{
			// In the linux version this doesnt get triggered when the wrong database is selected!!!
			// It just says cant query the server!!!
			// Why is this getting skipped!!!!!!!!!!!!!
			printf( "Can't select the %s database !\n", mySQL_Database ) ;
			mysql_close( myData ) ;
			return 2 ;
		}
	}
	else {
		printf( "Can't connect to the mysql server (%s) on port %d !\nmysql_error = %s\n",
			mySQL_Host, mySQL_Port, mysql_error(myData) ) ;

		mysql_close( myData ) ;
		return 1 ;
	}
#endif
	printf ("Tethealla Server Account Addition\n");
	printf ("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
	pw_ok = 0;
	while (!pw_ok)
	{
		printf ("New account's username: ");
		scanf ("%s", inputstr );
		if (strlen(inputstr) < 17)
		{
#ifdef NO_SQL
			pw_ok = 1;
			for (ds=0;ds<num_accounts;ds++)
			{
				if (!strcmp(&inputstr[0],&account_data[ds]->username[0]))
				{
					pw_ok = 0;
					break;
				}
			}
			if (!pw_ok)
				printf ("There's already an account by that name on the server.\n");
#endif
#ifndef NO_SQL
			sprintf (&myQuery[0], "SELECT * from account_data WHERE username='%s'", inputstr );
			
			// Check to see if that account already exists.
			if ( ! mysql_query ( myData, &myQuery[0] ) )
			{
				myResult = mysql_store_result ( myData );
				num_rows = (int) mysql_num_rows ( myResult );
				if (num_rows)
				{
					printf ("There is already an account by that name on the server.\n");
					myRow = mysql_fetch_row ( myResult );
					max_fields = mysql_num_fields ( myResult );
					printf ("Row data:\n");
					printf ("-=-=-=-=-\n");
					for (ch=0;ch<max_fields;ch++)
						printf ("column %u = %s\n", ch, myRow[ch] );
				}
				else
					pw_ok = 1;
				mysql_free_result ( myResult );
			}
			else
			{
				printf ("Couldn't query the MySQL server.\n");
				return 1;
			}
#endif
		}
		else
			printf ("Desired account name length should be 16 characters or less.\n");
	}
	memcpy (&username[0], &inputstr[0], strlen (inputstr)+1);
	// Gunna use this to salt it up
	regtime = time(NULL);
	pw_ok = 0;
	while (!pw_ok)
	{
		printf ("New account's password: ");
		scanf ("%s", inputstr );
		if ( ( strlen (inputstr ) < 17 ) || ( strlen (inputstr) < 8 ) )
		{
			memcpy (&password[0], &inputstr[0], 17 );
			pw_ok = 1;
		}
		else
			printf ("Desired password length should be 16 characters or less.\n");
	}
	pw_ok = 0;
	while (!pw_ok)
	{
		printf ("New account's e-mail address: ");
		scanf ("%s", inputstr );
		memcpy (&email[0], &inputstr[0], strlen (inputstr)+1 );
		// Check to see if the e-mail address has already been registered to an account.
#ifdef NO_SQL
			pw_ok = 1;
			for (ds=0;ds<num_accounts;ds++)
			{
				if (!strcmp(&email[0],&account_data[ds]->email[0]))
				{
					pw_ok = 0;
					break;
				}
			}
			if (!pw_ok)
			{
				printf ("That e-mail address is already in use.\n");
				num_rows = 1;
			}
			else
				num_rows = 0;
#endif
#ifndef NO_SQL
		// WARNING!! This does not stop people from making new accounts with the same email as it counts different
		// letter cases as unique emails. For example, email@gmail.com =/= Email@gmail.com!!
		sprintf (&myQuery[0], "SELECT * from account_data WHERE email='%s'", email );
		//printf ("Executing MySQL query: %s\n", myQuery );
		if ( ! mysql_query ( myData, &myQuery[0] ) )
		{
			myResult = mysql_store_result ( myData );
			num_rows = (int) mysql_num_rows ( myResult );
			mysql_free_result ( myResult );
			if (num_rows)
				printf ("That e-mail address has already been registered to an account.\n");
		}
		else
		{
			printf ("Couldn't query the MySQL server.\n");
			return 1;
		}
#endif
		if (!num_rows) // If not in the database already
		{
			pw_ok = 1;
		}
	}
#ifdef NO_SQL
	num_rows = num_accounts;
#endif
#ifndef NO_SQL
	// Check to see if any accounts already registered in the database at all.
	sprintf (&myQuery[0], "SELECT * from account_data" );
	
	// Check to see if the e-mail address has already been registered to an account.
	if ( ! mysql_query ( myData, &myQuery[0] ) )
	{
		myResult = mysql_store_result ( myData );
		num_rows = (int) mysql_num_rows ( myResult );
		mysql_free_result ( myResult );
		printf ("There are %i accounts already registered on the server.\n", num_rows );
	}
	else
	{
		printf ("Couldn't query the MySQL server.\n");
		return 1;
	}
#endif
	reg_seconds = (unsigned) regtime / 3600L;
	ch = strlen (&password[0]);
	sprintf (&config_data[0], "%d", reg_seconds);
	sprintf (&password[ch], "_%s_salt", &config_data[0] );
	MDString (&password[0], &MDBuffer[0] );
	for (ch=0;ch<16;ch++)
		sprintf (&md5password[ch*2], "%02x", (unsigned char) MDBuffer[ch]);
	md5password[32] = 0;
	if (!num_rows)
	{
		guildcard_number = 42000001;	// First account created is always GM.
#ifdef NO_SQL
		account_data[num_accounts] = malloc ( sizeof ( L_ACCOUNT_DATA ) );
		memset (account_data[num_accounts], 0, sizeof ( L_ACCOUNT_DATA ) );
		memcpy (&account_data[num_accounts]->username, &username, 18 );
		memcpy (&account_data[num_accounts]->password, &md5password, 33 );
		memcpy (&account_data[num_accounts]->email, &email, 255 );
		account_data[num_accounts]->regtime = reg_seconds;
		account_data[num_accounts]->guildcard = 42000001;
		account_data[num_accounts]->isactive = 1;
		account_data[num_accounts]->isgm = 1;
		account_data[num_accounts]->teamid = -1;
		UpdateDataFile ( "account.dat", 0, account_data[num_accounts], sizeof ( L_ACCOUNT_DATA ), 1 );
#endif
#ifndef NO_SQL
		sprintf (&myQuery[0], "INSERT into account_data (username,password,email,regtime,guildcard,isgm,isactive) VALUES ('%s','%s','%s','%u','%u','1','1')", username, md5password, email, reg_seconds, guildcard_number );
#endif
	}
	else
	{
#ifdef NO_SQL
		for (ds=0;ds<num_accounts;ds++)
		{
			if (account_data[ds]->guildcard > highid)
				highid = account_data[ds]->guildcard;
		}
		highid++;
		account_data[num_accounts] = malloc ( sizeof ( L_ACCOUNT_DATA ) );
		memset (account_data[num_accounts], 0, sizeof ( L_ACCOUNT_DATA ) );
		memcpy (&account_data[num_accounts]->username, &username, 18 );
		memcpy (&account_data[num_accounts]->password, &md5password, 33 );
		memcpy (&account_data[num_accounts]->email, &email, 255 );
		account_data[num_accounts]->regtime = reg_seconds;
		account_data[num_accounts]->isactive = 1;
		account_data[num_accounts]->guildcard = highid;
		account_data[num_accounts]->isgm = 0;
		account_data[num_accounts]->teamid = -1;
		UpdateDataFile ( "account.dat", num_accounts, account_data[num_accounts], sizeof ( L_ACCOUNT_DATA ), 1 );
#endif
#ifndef NO_SQL
		sprintf (&myQuery[0], "INSERT into account_data (username,password,email,regtime,isactive) VALUES ('%s','%s','%s','%u','1')", username, md5password, email, reg_seconds );
#endif
	}
	// Insert into table.
#ifdef NO_SQL
	printf ("Account added.");
#endif
#ifndef NO_SQL
	if ( ! mysql_query ( myData, &myQuery[0] ) )
		printf ("Account successfully added to the database!\n");
	else
	{
		printf ("Couldn't query the MySQL server.\n");
		return 1;
	}
	mysql_close( myData ) ;
#endif
	return 0;
}
