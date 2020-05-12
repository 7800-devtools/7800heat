#define PROGNAME "7800heat v0.1"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

//must be a power of two...
#define MAXBANKS 32

void GetLabel(char *line, char *label);
void GetContents(char *line, char *contents);
void usage(char *programname);
int CheckTightLoop(int address,char *contents1,char *contents2);

char *TraceFileName, *OutFileName;
FILE *TraceFileHandle, *OutFileHandle;
int SuperGame = FALSE;
int MaxBankFound = 0;
int LowRomUsed = FALSE;
int IgnoreTightLoop = TRUE;
int Bank=0;

int main(int argc, char **argv)
{

	int errflg = 0;
	int c;
	int i,j;


	TraceFileName=NULL;
	OutFileName=NULL;

	if (argc == 1)
		errflg++;

	while ((c = getopt(argc, argv, ":o:t:f")) != -1)
	{
		switch (c)
		{
			case 'o':
				OutFileName = optarg;
				break;
			case 't':
				TraceFileName = optarg;
				break;
			case 'f':
				IgnoreTightLoop = FALSE;
				break;
			case '?':
				fprintf(stderr, "ERR: unrecognised option \"-%c\"\n", optopt);
				errflg++;
				break;
			default:
				errflg++;
				break;
		}
        }

	if (TraceFileName == NULL)
	{
		errflg++;
		fprintf(stderr,"ERR: trace file name contents missing\n");
	}
	if (OutFileName == NULL)
	{
		errflg++;
		fprintf(stderr,"ERR: output file name contents missing\n");
	}
	if(errflg==0)
	{
		TraceFileHandle = fopen(TraceFileName,"rb");
		OutFileHandle   = fopen(OutFileName,"w");

		if(TraceFileHandle == NULL)
		{
			errflg++;
			fprintf(stderr,"ERR: trace file \'%s\'couldn't be opened\n",TraceFileName);
		}
		if(OutFileHandle == NULL)
		{
			errflg++;
			fprintf(stderr,"ERR: output file \'%s\'couldn't be opened\n",OutFileName);
		}

	}
	if (errflg)
	{
		usage(argv[0]);
		return (2);
	}

	char linebuffer[1024],label[1024],contents[1024];

	int EOFReached=FALSE;
	int LowAddressFlag=FALSE;

	fprintf(stderr,"Searching for \"jmp ($fffc)\" handoff from bios to cart...\n");

	for(;;)
	{
		if(fgets(linebuffer,1024,TraceFileHandle)==NULL)
		{
			EOFReached=TRUE;
			break;
		}
		
		GetLabel(linebuffer,label);
		if(!label[0]) // no BANK: or ADDR: ?!?
			continue;
		GetContents(linebuffer,contents);
		if(strstr(contents,"jmp ($fffc)")!=NULL)
		{
			break;
		}
	}

	if(EOFReached)
	{
		fprintf(stderr,"  ...handoff not found. Exiting.\n");
		fprintf(stderr,"     be sure to capture your trace prior to console boot.\n");
		exit(1);
	}
	fprintf(stderr,"  ...handoff found.\n");

	long int BankCountLow=0;
	long int BankCountMid[MAXBANKS];
	memset(&BankCountMid,0,MAXBANKS*sizeof(int));
	long int BankCountHi=0;

	int *count_lowrom = calloc(0x8000,sizeof(int)); 		// counts for addresses 0000-7fff (static)
	int *count_midrom[MAXBANKS];
	for(i=0;i<MAXBANKS;i++)
		count_midrom[i]= calloc(0x4000,sizeof(int)); 		// counts for addresses 8000-bfff (banks)
	int *count_hirom = calloc(0x4000,sizeof(int)); 			// counts for addresses c000-ffff (static)

	char **assembly_lowrom = calloc(0x8000,sizeof(char *));		// assembly at 0000-7fff (static)
	char **assembly_midrom[MAXBANKS];
	for(i=0;i<MAXBANKS;i++)
		assembly_midrom[i] = calloc(0x4000,sizeof(char *));	// assembly at 8000-bfff (banks)
	char **assembly_hirom = calloc(0x4000,sizeof(char *)); 		// assembly at c000-ffff (static)

	int LastAddress = 0;

	fprintf(stderr,"Parsing trace file, tallying counts, making tea...\n");
	for(;;)
	{
		if(fgets(linebuffer,1024,TraceFileHandle)==NULL)
		{
			EOFReached=TRUE;
			break;
		}
		GetLabel(linebuffer,label);
		GetContents(linebuffer,contents);
		if( (label[0]) && (strncmp(label,"BANK",4)==0))
		{
			SuperGame=TRUE;
			Bank=strtol(contents, NULL, 16);
			if(Bank>(MAXBANKS-1))
			{
				fprintf(stderr,"Warning: maximum banks exceeded.\nAnalysis may be innacurate.\n");
				Bank=Bank&(MAXBANKS-1);	
			}
			if(Bank>MaxBankFound)
				MaxBankFound=Bank;
		}
		else if(label[0]) // default is it's an address. increment the address index, save the asm
		{
			int address=strtol(label, NULL, 16);

			//fprintf(stderr,"DEBUG  %06X: %s\n",address,contents);
			if( (address<0x3000) && (LowAddressFlag==FALSE) )
			{
				fprintf(stderr,"Warning: code seems to be running below 0x3000. Ram-based routines?\n         %s: %s",label,contents);
				LowAddressFlag=TRUE;
			}
			if (address>0xffff)
			{
				fprintf(stderr,"Error: an instruction ran outside of 6502 address space? (%06X: %s)\n",address,contents);
				exit(1);
			}
			else if (address<0x8000)
			{
				// low address range
				count_lowrom[address]++;
				if(!assembly_lowrom[address])
				{
					assembly_lowrom[address]=malloc(32);
					strncpy(assembly_lowrom[address],contents,32);
				}
			}
			else if ( (address>=0x8000) && (address<0xc000) )
			{
				// mid, possibly-banked, address range
				int absaddress = address - 0x8000;
				count_midrom[Bank][absaddress]++;
				if(!assembly_midrom[Bank][absaddress])
				{
					assembly_midrom[Bank][absaddress]=malloc(32);
					strncpy(assembly_midrom[Bank][absaddress],contents,32);
				}
			}
			else if ( (address>=0xc000) && (address<0x10000) )
			{
				// hi address range
				int absaddress = address - 0xc000;
				count_hirom[absaddress]++;
				if(!assembly_hirom[absaddress])
				{
					assembly_hirom[absaddress]=malloc(32);
					strncpy(assembly_hirom[absaddress],contents,32);
				}
			}
			else
			{
				fprintf(stderr,"Error: unknown parsing error\n");
				exit(1);
			}
		}
	} // for loop for parsing the trace file
	fprintf(stderr,"  ...done\n");


	// find the MAX number of times any particular opcode was run
	int MaxCount = 0;
	for(i=0;i<0x8000;i++)
	{
		if(count_lowrom[i]>0)
		{
			if((IgnoreTightLoop)&&(i<0x7ffd)&&(count_lowrom[i+2]>0))
			{
				if(CheckTightLoop(i,assembly_lowrom[i],assembly_lowrom[i+2]))
				{
					count_lowrom[i]*=-1;
					count_lowrom[i+2]*=-1;
				}
			}
			BankCountLow=BankCountLow+count_lowrom[i];
		}
		if(count_lowrom[i]>MaxCount)
			MaxCount=count_lowrom[i];
		if(count_lowrom[i]>0)
			BankCountLow=BankCountLow+count_lowrom[i];
		if ( (LowRomUsed==FALSE) && (count_lowrom[i]>0) )
			LowRomUsed=TRUE;
	}
	for(i=0;i<0x4000;i++)
	{
		if(count_hirom[i]>0)
		{
			if((IgnoreTightLoop)&&(i<0x3ffd)&&(count_hirom[i+2]>0))
			{
				if(CheckTightLoop(i+0xc000,assembly_hirom[i],assembly_hirom[i+2]))
				{
					count_hirom[i]*=-1;
					count_hirom[i+2]*=-1;
				}
			}

			BankCountHi=BankCountHi+count_hirom[i];
		}
		if(count_hirom[i]>MaxCount)
			MaxCount=count_hirom[i];
	}
	for(i=0;i<0x4000;i++)
		for(j=0;j<MAXBANKS;j++)
		{
			if(count_midrom[j][i]>0)
			{
				if((IgnoreTightLoop)&&(i<0x3ffd)&&(count_midrom[j][i+2]>0))
				{
					if(CheckTightLoop(i+0x8000,assembly_midrom[j][i],assembly_midrom[j][i+2]))
					{
						count_midrom[j][i]*=-1;
						count_midrom[j][i+2]*=-1;
					}
				}
				BankCountMid[j]+=count_midrom[j][i];
			}
			if(count_midrom[j][i]>MaxCount)
				MaxCount=count_midrom[j][i];
		}

	// Make the HTML Disassembly + Heatmap
	fprintf(OutFileHandle,"<html><head><title>%s 7800heat profile</title></head>\n<body>\n",OutFileName);

	// some CSS...
	//fprintf(OutFileHandle,"<style type = text/css>\n");
	
	fprintf(OutFileHandle,"<h1>%s 7800heat profile</h1>\n",OutFileName);

	fprintf(OutFileHandle,"<h2>Stats and Info</h2>\n");

	if(SuperGame)
		fprintf(OutFileHandle,"Bank Switching: SuperGame<br>\n");
	else
		fprintf(OutFileHandle,"Bank Switching: None/Flat<br>\n");
	fprintf(OutFileHandle,"Maximum Instruction Count Found: %d<br>\n",MaxCount);
	fprintf(OutFileHandle,"Tight Register Loops: ");
	if(IgnoreTightLoop)
		fprintf(OutFileHandle,"Ignored (negative count reported)<br>\n");
	else
		fprintf(OutFileHandle,"Counted<br>\n");

	int R,G,B;
	long ctmp;
	
	double MaxCountLog = log((double)MaxCount); 
	if(!SuperGame)
		fprintf(OutFileHandle,"<h2>Game Code</h2>\n");

	if(LowRomUsed)
	{
		if(SuperGame)
			fprintf(OutFileHandle,"<h2>Bank N-1 at $4000</h2>\n");

		LastAddress=0;
		fprintf(OutFileHandle,"<pre>\n");
		for(i=0;i<0x8000;i++)
		{
			if(count_lowrom[i]!=0)
			{
				if(i-LastAddress>8)
					fprintf(OutFileHandle,"\n");
				LastAddress=i;
				ctmp=(log((double)count_lowrom[i])*128)/MaxCountLog;
				if(ctmp<0)
					ctmp=0;
				R=ctmp+127; B=255-ctmp; G=128;
				fprintf(OutFileHandle,"<span style=\"background-color:#%02X%02X%02x;\">",R,G,B);
				fprintf(OutFileHandle,"%04X: %-14s (%d)",i,assembly_lowrom[i],count_lowrom[i]);
				fprintf(OutFileHandle,"</span>\n");
			}
		}
		fprintf(OutFileHandle,"</pre>\n");
	}
	for(j=0;j<=MaxBankFound;j++)
	{
		if(SuperGame)
			fprintf(OutFileHandle,"<h2>Bank %d at $8000</h2>\n",j);
		fprintf(OutFileHandle,"<pre>\n");
		LastAddress=0;
		for(i=0;i<0x4000;i++)
		{
			if(count_midrom[j][i]!=0)
			{
				if(i-LastAddress>8)
					fprintf(OutFileHandle,"\n");
				LastAddress=i;
				ctmp=(log((double)count_midrom[j][i])*128)/MaxCountLog;
				if(ctmp<0)
					ctmp=0;
				R=ctmp+127; B=255-ctmp; G=128;
				fprintf(OutFileHandle,"<span style=\"background-color:#%02X%02X%02x;\">",R,G,B);
				fprintf(OutFileHandle,"%04X: %-14s (%d)",i+0x8000,assembly_midrom[j][i],count_midrom[j][i]);
				fprintf(OutFileHandle,"</span>\n");
			}
		}
		fprintf(OutFileHandle,"</pre>\n");
	}
	if(SuperGame)
		fprintf(OutFileHandle,"<h2>Bank N at $C000</h2>\n");
	fprintf(OutFileHandle,"<pre>\n");
	LastAddress=0;
	for(i=0;i<0x4000;i++)
	{
		if(count_hirom[i]!=0)
		{
			if(i-LastAddress>8)
				fprintf(OutFileHandle,"\n");
			LastAddress=i;
			ctmp=(log((double)count_hirom[i])*128)/MaxCountLog;
			if(ctmp<0)
				ctmp=0;
			R=ctmp+127; B=255-ctmp; G=128;
			fprintf(OutFileHandle,"<span style=\"background-color:#%02X%02X%02x;\">",R,G,B);
			fprintf(OutFileHandle,"%04X: %-14s (%d)",i+0xc000,assembly_hirom[i],count_hirom[i]);
			fprintf(OutFileHandle,"</span>\n");
		}
	}
	fprintf(OutFileHandle,"</pre>\n");
	fprintf(OutFileHandle,"</body></html>\n");
	fclose(OutFileHandle);

}

int CheckTightLoop(int address1,char *contents1,char *contents2)
{

	// tight loops always start with a register or memory load or bit...
	if( (strncmp(contents1,"ld",2)!=0) && (strncmp(contents1,"bit",3)!=0) )
		return(FALSE);
	if( 	(strncmp(contents2,"bpl",2)==0) || (strncmp(contents2,"bmi",2)==0) || 
		(strncmp(contents2,"bvc",2)==0) || (strncmp(contents2,"bcc",2)==0) || 
		(strncmp(contents2,"bcs",2)==0) || (strncmp(contents2,"bne",2)==0) || 
		(strncmp(contents2,"beq",2)==0) )
	{
		int address2;
		char *delim;
		delim=strchr(contents2,'$');
		if(delim==NULL)
			return(FALSE);
		delim++;
		address2=strtol(delim,NULL,16);
		if(address1==address2)
		{
			return(TRUE);
		}
	}
	return(FALSE);
}

void GetLabel(char *line, char *label)
{

	// pull the first word from the file, validate it has a colon, and
	// then update label with it's value

	int t;

	if(!line[0]) // check if line is empty
	{
		label[0]=0;
		return;
	}

	for(t=0;t<(strlen(line));t++)
	{
		if(line[t]==':')
			break;
	}
	if(t==strlen(line))
	{
		// there was no ":" in the whole string. it's malformed.
		label[0]=0;
		return;
	}
	strcpy(label,line);
	label[t]=0;
	for(t=0;t<strlen(label);t++)
		if ((label[t]=='\n') || (label[t]=='\r'))
			label[t]=0;
}

void GetContents(char *line, char *contents)
{
	// pull everything past the first word from the line
	// then update contents with it

	if(!line[0]) // check if line is empty
	{
		contents[0]=0;
		return;
	}

	int t;

	for(t=0;t<(strlen(line));t++)
	{
		if(line[t]==':')
			break;
	}
	if(t==strlen(line))
	{
		// there was no ":" in the whole string. it's malformed.
		contents[0]=0;
		return;
	}
	t++;
	for(;t<strlen(line);t++)
	{
		if(line[t]!=' ')
			break;
	}
	strcpy(contents,line+t);
	for(t=0;t<strlen(contents);t++)
		if ((contents[t]=='\n') || (contents[t]=='\r'))
			contents[t]=0;
}


void usage(char *programname)
{
    fprintf(stderr, "%s %s %s\n", PROGNAME, __DATE__, __TIME__);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s -t TRACEFILE.trc -o OUTFILE.html [-f]\n", programname);
    fprintf(stderr, "\n");
    fprintf(stderr, "  -t TRACEFILE.trc specifies the trace file to parse.\n");
    fprintf(stderr, "  -o OUTFILE.html specifies the output html heatmap.\n");
    fprintf(stderr, "  -f specifies full trace counts (include tight loops).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "To generate the trace file:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  1.  Launch the game in the a7800 debugger:\n");
    fprintf(stderr, "        a7800 a7800 -cart mygame.a78 -debug\n");
    fprintf(stderr, "  2.  In the debug window type:\n");
    fprintf(stderr, "        trace gamename.trc,0,noloop\n");
    fprintf(stderr, "  2a. If your game is SuperGame format, type:\n");
    fprintf(stderr, "        wpset 8000,4000,w,1==1,{tracelog \"BANK: %%02X\\n\",wpdata;g}\n");
    fprintf(stderr, "  3.  Hit F5 in the debug window to run the game, and play for a few minutes.\n");
    fprintf(stderr, "  4.  Hit F11 in the debug window to pause execution, and type:\n");
    fprintf(stderr, "        trace off.\n");
    fprintf(stderr, "\n");
}

