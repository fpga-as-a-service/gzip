/** QuickPlay
 *
 *  gzip_fpga test application implementation file
 */

/* Standard includes */
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>       // for mmap calls
#include <dirent.h>         // for directory scanning
#include <iomanip>          // for cout alignement
#include <chrono>           // for time measurement
#include <thread>           // for std:thread
#include <algorithm>        // for strip()
#include "TextTable.h"      // for console table drawing

/* QuickPlay API library include */
#include <QpDesign.h>

#define QAPP_VERSION            "1.1.0"

/* Define accelerator to use */
#define DEVICE_NAME             "gzip_highcompr_quickapp"
#define JSON_SEARCH_PATH        "/opt/accelize/quickapps/apps/gzip_highCompr/bitstream/"
#define LIC_SEARCH_PATH         "/opt/accelize/quickapps/apps/gzip_highCompr/bitstream/"
#define SAMPLE_FILES_PATH       "/opt/accelize/quickapps/apps/gzip_highCompr/applications/sample_files/"

#define MAX_FILENAME_SIZE       65536
#define MIN_FIFO_SIZE           65536
#define SIZE_1KB                0x400
#define SIZE_1MB                0x100000
#define SIZE_1GB                0x40000000
#define PCIE_FIFO_SIZE          0x20000000
#define BW_TEST_ITERATION_CNT   100

// Select MODE: SGDMAR or SGDMA
#define SGDMAR
#ifdef SGDMAR
#define RW_SIZE_LIMIT           0xFFFFFFFF
#endif

using namespace std;
using namespace QuickPlayLib;

char            *output_file;
char            *input_file;
long long int   infsize, outfsize, outfsizeMAX;

/* QuickPlay Device */
QpDesign dev1;

/* Boolean variable to let HWLogger to exit */
bool hwLoggerExit = false;

string sampleInFolderPath_gzip   = string(SAMPLE_FILES_PATH)+string("gzip_input_files");

typedef struct {
    QpStream *pStream;
    bool     quiet;
    char     *pBuffer;
    long long int reqTransfSize;
    long long int realTransfSize;
    double	 bwMeasure;
    double	 elapsedSecs;
    unsigned int loopCnt;
}thread_params_t, *PThreadParams;

typedef struct {
    bool    operateOnFolder;
    bool    quiet;
    bool    verifyIntegrity;
    bool    OScompare;
    bool    demoMode;
    bool    force;
    bool    verbose;
    string  path;
    bool 	writeCSV;
} gzip_args_t;

typedef struct {
    std::string     filename;           //table: filename
    std::string     comprResult;        //table: result

    double          hwBwMBps;           //table: BC: HW Gzip
    double          swBwFastMBps;       //table: BC: SW Gzip --fast
    double          swBwBestMBps;       //table: BC: SW Gzip --best
    double          bwFastGain;         //table: BC: Gain vs SW Gzip --fast
    double          bwBestGain;         //table: BC: Gain vs SW Gzip --fast

    double          hwComprRatio;       //table: CC: HW Gzip
    double          swComprFastRatio;   //table: CC: SW Gzip --fast
    double          swComprBestRatio;   //table: CC: SW Gzip --best
    double          comprFastGain;      //table: CC: Gain vs SW Gzip --fast
    double          comprBestGain;      //table: CC: Gain vs SW Gzip --fast
} file_results_t;

/**
 *  getFileSize
 */
long long int getFileSize(string filePath)
{
    struct stat st;
	stat(filePath.c_str(), &st);
	return st.st_size;
}

/**
 *  getFileSizeStr
 */
string getFileSizeStr(string filePath)
{
    double byteSize = (double)getFileSize(filePath);
    string unit = " B";
    
    if(byteSize  < (double)SIZE_1KB)
        unit = " B";
    else if(byteSize  < (double)SIZE_1MB) {
        byteSize /= (double)SIZE_1KB;
        unit = " KB";
    }
    else if(byteSize  < (double)SIZE_1GB) {
        byteSize /= (double)SIZE_1MB;
        unit = " MB";
    }
    else {
        byteSize /= (double)SIZE_1GB;
        unit = " GB";
    }

    stringstream stream;
    stream << "(" << fixed << setprecision(2) << byteSize << unit << ")";
    return(stream.str());
}

/**
 *  getBandwidthMBps
 */
double getBandwidthMBps(chrono::time_point<chrono::system_clock> start, chrono::time_point<chrono::system_clock> end, long long int nbBytes)
{
    std::chrono::duration<double> elapsed_seconds = end-start;
    return nbBytes/elapsed_seconds.count()/SIZE_1MB;
}

/**
 *  getElapsedSecs
 */
double getElapsedSecs(chrono::time_point<chrono::system_clock> start, chrono::time_point<chrono::system_clock> end)
{
    std::chrono::duration<double> elapsed_seconds = end-start;
    return elapsed_seconds.count();
}

/**
 *  std::string strip()
 */
std::string strip(std::string str)
{
  str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
  return str;
}

/**
 *  basename
 */
string basename(string path)
{
    char sep = '/';
#ifdef _WIN32
   sep = '\\';
#endif
   size_t i = path.rfind(sep, path.length());
   if (i != string::npos) {
      return(path.substr(i+1, path.length() - i));
   }
   return(path);
}

/**
 *  Print results as a console colored table
 */
void display_result_table(file_results_t* resTable, unsigned int nbFiles)
{
    // Bandwidth Comparison Table
    TextTable tableBw( '-', '|', '+' );
    tableBw.setTitle("BANDWIDTH COMPARISON (units are MB/s)");
    tableBw.add( "Filename" );
    tableBw.add( "Result" );
    tableBw.add( "HW" );
    tableBw.add( "SW --fast" );
    tableBw.add( "SW --best" );
    tableBw.add( "Gain (vs fast)" );
    tableBw.add( "Gain (vs best)" );
    tableBw.endOfRow();
    for(unsigned int i=0; i<nbFiles; i++) {
        tableBw.add( resTable[i].filename );
        tableBw.add( resTable[i].comprResult );
        tableBw.add( resTable[i].hwBwMBps );
        tableBw.add( resTable[i].swBwFastMBps );
        tableBw.add( resTable[i].swBwBestMBps );
        tableBw.add( resTable[i].bwFastGain );
        tableBw.add( resTable[i].bwBestGain );
        tableBw.endOfRow();
    }
    tableBw.setAlignment( 2, TextTable::Alignment::LEFT );
    std::cout << "\n" << tableBw;

    // Compression Comparison Table
    TextTable tableCompr( '-', '|', '+' );
    tableCompr.setTitle("COMPRESSION RATIO COMPARISON");
    tableCompr.add( "Filename" );
    tableCompr.add( "Result" );
    tableCompr.add( "HW" );
    tableCompr.add( "SW --fast" );
    tableCompr.add( "SW --best" );
    tableCompr.add( "Gain (vs fast)" );
    tableCompr.add( "Gain (vs best)" );
    tableCompr.endOfRow();
    for(unsigned int i=0; i<nbFiles; i++) {
        tableCompr.add( resTable[i].filename );
        tableCompr.add( resTable[i].comprResult );
        tableCompr.add( resTable[i].hwComprRatio );
        tableCompr.add( resTable[i].swComprFastRatio );
        tableCompr.add( resTable[i].swComprBestRatio );
        tableCompr.add( resTable[i].comprFastGain );
        tableCompr.add( resTable[i].comprBestGain );
        tableCompr.endOfRow();
    }
    tableCompr.setAlignment( 2, TextTable::Alignment::LEFT );
    std::cout << "\n" << tableCompr;
    

    // Compute Average and Max Throughput
    double avBwMBps=0.0, mxBwMBps=0.0;
    for(unsigned int i=0; i<nbFiles; i++) {
        avBwMBps += resTable[i].hwBwMBps;
        if(resTable[i].hwBwMBps>mxBwMBps) mxBwMBps=resTable[i].hwBwMBps;
    }
    avBwMBps /= nbFiles;
    std::cout << "Average Throughput " << (avBwMBps) << " MB/s" << std::endl;
    std::cout << "Maximal Throughput " << (mxBwMBps) << " MB/s" << std::endl;
}

/**
 *  Save results in CSV file
 */
int save_result_table_csvfile(file_results_t* resTable, unsigned int nbFiles)
{
	ofstream myfile;
	std::string bwFilepath ("./gzip_bandwidth_comparison.csv");
	std::string comprFilepath("./gzip_compression_comparison.csv");
	std::string webpageDisplayPath("./webpage_display.csv");
	
	// Compute Best Bw and Best Compression
    double mxBwMBps=0.0, mxComprRatio=0.0;
    for(unsigned int i=0; i<nbFiles; i++) {
        if(resTable[i].hwBwMBps>mxBwMBps) mxBwMBps=resTable[i].hwBwMBps;
        if(resTable[i].hwComprRatio>mxComprRatio) mxComprRatio=resTable[i].hwComprRatio;
    }
    myfile.open(webpageDisplayPath.c_str());
    if(!myfile) {
		std::cout << KRED << "Error creating CSV file " << bwFilepath << KNRM << std::endl;
		return -1;
    } 
    myfile << mxBwMBps << ',' << mxComprRatio;
    myfile.close();
	
	// Bandwith
	myfile.open(bwFilepath.c_str());
    if(!myfile) {
		std::cout << KRED << "Error creating CSV file " << bwFilepath << KNRM << std::endl;
		return -1;
    } 
    myfile << "BANDWIDTH COMPARISON (units are MB/s)\n";
    myfile << "Filename,";
    myfile << "Result," ;
    myfile << "HW,";
    myfile << "SW --fast,";
    myfile << "SW --best,";
    myfile << "Gain (vs fast),";
    myfile << "Gain (vs best)\n"; 
    for(unsigned int i=0; i<nbFiles; i++) {
		myfile << resTable[i].filename << ',';
		myfile << resTable[i].comprResult << ',';
		myfile << resTable[i].hwBwMBps << ',';
		myfile << resTable[i].swBwFastMBps << ',';
		myfile << resTable[i].swBwBestMBps << ',';
		myfile << resTable[i].bwFastGain << ',';
		myfile << resTable[i].bwBestGain << '\n';
	}	
	myfile.close();
	
	// Compression ratio
	myfile.open(comprFilepath.c_str());
    if(!myfile) {
		std::cout << KRED << "Error creating CSV file " << comprFilepath << KNRM << std::endl;
		return -1;
    } 
    myfile << "COMPRESSION RATIO COMPARISON\n";
    myfile << "Filename,";
    myfile << "Result," ;
    myfile << "HW,";
    myfile << "SW --fast,";
    myfile << "SW --best,";
    myfile << "Gain (vs fast),";
    myfile << "Gain (vs best)\n";   
    for(unsigned int i=0; i<nbFiles; i++) {
		myfile << resTable[i].filename << ',';
		myfile << resTable[i].comprResult << ',';
		myfile << resTable[i].hwComprRatio << ',';
		myfile << resTable[i].swComprFastRatio << ',';
		myfile << resTable[i].swComprBestRatio << ',';
		myfile << resTable[i].comprFastGain << ',';
		myfile << resTable[i].comprBestGain << '\n';
	}	
	myfile.close();
	
	return 0;
}

/**
 *  test if path is a folder
 */
int isFolder(string path)
{
    struct stat s;
    if( stat(path.c_str(),&s) == 0 ) {
        if( s.st_mode & S_IFDIR )
            return true;
    }
    return false;
}

/**
 *  test if path is a file
 */
int isFile(string path)
{
    struct stat s;
    if( stat(path.c_str(),&s) == 0 ) {
        if( s.st_mode & S_IFREG )
            return true;
    }
    return false;
}

/**
 *  test if path is a GZip Archive File
 */
int isGzipArchive(string path)
{
    if(!path.compare(path.size()-3, 3, ".gz"))
        return true;
    return false;
}

/**
 *  test if path is a JSON File
 */
int isJsonFile(string path)
{
    if(!path.compare(path.size()-5, 5, ".json"))
        return true;
    return false;
}

/**
 *  test if path is a License File
 */
int isLicenseFile(string path)
{
    if(!path.compare(path.size()-4, 4, ".lic"))
        return true;
    return false;
}

/**
 * Producer SGDMAR thread
 */
#ifdef SGDMAR
void tProducer_SGDMAR(PThreadParams pThreadParams)
{
    if(!pThreadParams){
        std::cerr << "tProducer: pThreadParams is NULL";
        return;
    }

    if(!pThreadParams->pStream){
        std::cerr << "tProducer: pStream member of pThreadParams is NULL";
        return;
    }

    std::chrono::time_point<std::chrono::system_clock> start = chrono::system_clock::now();
    for(unsigned int i=0; i< pThreadParams->loopCnt; i++) {
        pThreadParams->realTransfSize=0;
        while(pThreadParams->realTransfSize < pThreadParams->reqTransfSize) {
            if((pThreadParams->reqTransfSize-pThreadParams->realTransfSize) >= RW_SIZE_LIMIT) {
                dev1.qpWriteStream(*pThreadParams->pStream, &pThreadParams->pBuffer[pThreadParams->realTransfSize], RW_SIZE_LIMIT, false);
                pThreadParams->realTransfSize += RW_SIZE_LIMIT;
            }
            else {
                dev1.qpWriteStream(*pThreadParams->pStream, &pThreadParams->pBuffer[pThreadParams->realTransfSize], (unsigned int)(pThreadParams->reqTransfSize-pThreadParams->realTransfSize), true);
                pThreadParams->realTransfSize=pThreadParams->reqTransfSize;
            }
        }
    }
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    pThreadParams->bwMeasure = getBandwidthMBps(start, end, pThreadParams->realTransfSize);
    pThreadParams->elapsedSecs = getElapsedSecs(start, end);
}
/**
 * Consumer SGDMAR thread
 */
void tConsumer_SGDMAR(PThreadParams pThreadParams)
{
    bool eop=false;
    unsigned int readBytes=0;
    int err=0;

    if(!pThreadParams){
        std::cerr << "pThreadParams is NULL";
        return;
    }

    if(!pThreadParams->pStream){
        std::cerr << "pStream member of pThreadParams is NULL";
        return;
    }
    
    std::chrono::time_point<std::chrono::system_clock> start = chrono::system_clock::now();  
    for(unsigned int i=0; i< pThreadParams->loopCnt; i++) { 
        pThreadParams->realTransfSize=0;
        eop = false;
        while(!eop && !err) {
            err = dev1.qpReadStream(*pThreadParams->pStream, &pThreadParams->pBuffer[pThreadParams->realTransfSize], RW_SIZE_LIMIT, eop, readBytes);
            pThreadParams->realTransfSize += readBytes;
        }
        outfsize = pThreadParams->realTransfSize;
    }

    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    if(err)
        std::cerr << KRED << "Data Read from FPGA error. File content could be incorrect" << KNRM << std::endl;

    pThreadParams->bwMeasure = getBandwidthMBps(start, end, pThreadParams->realTransfSize);
    pThreadParams->elapsedSecs = getElapsedSecs(start, end);
}
#endif

/**
 * Producer SGDMA thread
 */
#ifdef SGDMA
void tProducer_SGDMA(PThreadParams pThreadParams)
{
    if(!pThreadParams){
        std::cerr << "tProducer: pThreadParams is NULL";
        return;
    }

    if(!pThreadParams->pStream){
        std::cerr << "tProducer: pStream member of pThreadParams is NULL";
        return;
    }

    // Retrieve Buffer Pointer
    char *pBuffer=NULL;
    pThreadParams->pStream->getStreamOption("dmaBuffer", pBuffer);

    // Define chunckSize
    long long int chunckSize = pThreadParams->reqTransfSize<PCIE_FIFO_SIZE?pThreadParams->reqTransfSize:PCIE_FIFO_SIZE;

    std::chrono::time_point<std::chrono::system_clock> start = chrono::system_clock::now();
    int err=0;
    for(unsigned int i=0; i< pThreadParams->loopCnt; i++) {
        pThreadParams->realTransfSize=0;
        while(pThreadParams->realTransfSize < pThreadParams->reqTransfSize) {
            if((pThreadParams->reqTransfSize-pThreadParams->realTransfSize) > chunckSize) {
                memcpy(pBuffer, &pThreadParams->pBuffer[pThreadParams->realTransfSize], chunckSize);
                err = dev1.qpWriteStream(*pThreadParams->pStream, NULL, chunckSize, false);
                pThreadParams->realTransfSize += chunckSize;
            }
            else {
                memcpy(pBuffer, &pThreadParams->pBuffer[pThreadParams->realTransfSize], (unsigned int)(pThreadParams->reqTransfSize-pThreadParams->realTransfSize));
                err = dev1.qpWriteStream(*pThreadParams->pStream, NULL, (unsigned int)(pThreadParams->reqTransfSize-pThreadParams->realTransfSize), true);
                pThreadParams->realTransfSize=pThreadParams->reqTransfSize;
            }
        }
    }
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    pThreadParams->bwMeasure = getBandwidthMBps(start, end, pThreadParams->realTransfSize*pThreadParams->loopCnt);
    pThreadParams->elapsedSecs = getElapsedSecs(start, end);
}
/**
 * Consumer SGDMA thread
 */
void tConsumer_SGDMA(PThreadParams pThreadParams)
{
    bool eop=false;
    unsigned int readBytes=0;
    int err=0;

    if(!pThreadParams){
        std::cerr << "pThreadParams is NULL";
        return;
    }

    if(!pThreadParams->pStream){
        std::cerr << "pStream member of pThreadParams is NULL";
        return;
    }

    // Retrieve Buffer Pointer
    char *pBuffer=NULL;
    pThreadParams->pStream->getStreamOption("dmaBuffer", pBuffer);

    // Define chunckSize
    long long int chunckSize = pThreadParams->reqTransfSize<PCIE_FIFO_SIZE?pThreadParams->reqTransfSize:PCIE_FIFO_SIZE;
    pThreadParams->realTransfSize=0;

    std::chrono::time_point<std::chrono::system_clock> start = chrono::system_clock::now();    
    for(unsigned int i=0; i< pThreadParams->loopCnt; i++) {
        eop = false;
        pThreadParams->realTransfSize=0;

        while(!eop && !err) {
           
            // FPGA READ
            std::cout << KRED << "starting qpReadStream => i=" << i << KNRM << std::endl;
            err = dev1.qpReadStream(*pThreadParams->pStream, NULL, chunckSize, eop, readBytes);
            std::cout << KRED << "qpReadStream => i=" << i << " - Byte rx = " << readBytes << " eop = " << (int)eop << KNRM << std::endl;
#if 1
            memcpy(&pThreadParams->pBuffer[pThreadParams->realTransfSize], pBuffer, readBytes);          
#else
            // Zero-copy hack
            int pfd1[2], pfd2[2];
            err = pipe( pfd1);
            if(err) {
                std::cerr << KRED << "Prod: Unable to create pipe 1" << KNRM << std::endl;
            }
            err = pipe( pfd2);
            if(err) {
                std::cerr << KRED << "Prod: Unable to create pipe 2" << KNRM << std::endl;
            }
            iovec param;

            // VMSPLICE pBuffer -> file_pipe
            param.iov_base = pBuffer;
            param.iov_len = readBytes;
            err = vmsplice(pfd1[1], &param, 1, 0);
            std::cout << KRED << "VMSPLICE pBuffer -> file_pipe => Byte tx = " << err << KNRM << std::endl;

            

            // VMSPLICE file_pipe -> user_buffer
            param.iov_base = &pThreadParams->pBuffer[pThreadParams->realTransfSize];
            param.iov_len = readBytes;
            err = vmsplice(pfd2[0], &param, 1, 0);
            std::cout << KRED << "VMSPLICE file_pipe -> user_buffer => Byte tx = " << err << KNRM << std::endl;

            

            close(pfd1[0]);
            close(pfd1[1]);
            close(pfd2[0]);
            close(pfd2[1]);
#endif            

            pThreadParams->realTransfSize += readBytes;
        }
        outfsize = pThreadParams->realTransfSize; 

        /*if(err) {
            std::cout << KRED << "Data Read from FPGA error. File content could be incorrect" << KNRM << std::endl;
        }*/
    }             
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
    
    pThreadParams->bwMeasure = getBandwidthMBps(start, end, pThreadParams->realTransfSize*pThreadParams->loopCnt);
    pThreadParams->elapsedSecs = getElapsedSecs(start, end);
}
#endif

/**
 * HwLogger thread
 */
void* tHwLogger()
{
	/* Press 'p' to print hardware information on the console */
	while(!hwLoggerExit) {
		char buf[256];
		fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
		sleep(1);
		if(read(0,buf,256)> 0 && buf[0]=='p') {
			dev1.qpPrintHwReport(std::cout, "file_in");
            dev1.qpPrintHwReport(std::cout, "archive_out");
        }
	}

	return NULL;
}

/**
 *  compare_md5sum
 */
int compare_md5sum(string filename1, string filename2)
{
	// if files does not exist
	struct stat buf;
	if (stat(filename1.c_str(), &buf) != 0)
		return -1;
	if (stat(filename2.c_str(), &buf) != 0)
		return -1;

	// Get md5sum values
	FILE *in1, *in2;
	char buff1[512], buff2[512];
    string cmd1 = "md5sum " + filename1;
    string cmd2 = "md5sum " + filename2;

	if(!(in1 = popen(cmd1.c_str(), "r")))
		return -2;
	
	fgets(buff1, sizeof(buff1), in1);
	pclose(in1);

	if(!(in2 = popen(cmd2.c_str(), "r")))
		return -2;
	
	fgets(buff2, sizeof(buff2), in2);
	pclose(in2);

	// Compare values
	if( strncmp(buff1, buff2, 32) !=0)
		return -3;

	return 0;
}

/**
 *  checkArchive
 */
int checkArchive(string inFile, string outFile, bool verbose)
{
    if(verbose)
        std::cout << KBLU << "Checking Archive " << outFile << " ..." << KNRM << std::endl;

    // if files does not exist
	struct stat buf;
	if (stat(inFile.c_str(), &buf) != 0) {
        std::cerr << KRED << "Error: Unable to open file [" << inFile << "]" << KNRM << std::endl;
		return -1;
	}
	if (stat(outFile.c_str(), &buf) != 0) {
        std::cerr << KRED << "Error: Unable to open file [" << outFile << "]" << KNRM << std::endl;
		return -1;
	}

    // Extract archive
    string extrFile = "/var/tmp/" + basename(inFile);
    FILE *in;
    string cmd = "gunzip -c " + outFile + " > " + extrFile + " 2> /dev/null";;
    if(!(in = popen(cmd.c_str(), "r"))){
		return -2;
	}
	pclose(in);

    // Compare md5sum
    int retCode = compare_md5sum(inFile, extrFile);

    // Clean Generated File
    cmd = "rm -f " + extrFile;
    in = popen(cmd.c_str(), "r");
    pclose(in);

    return retCode;
}

/**
 *  Display Application Configuration
 */
void show_appconfig(gzip_args_t args)
{
    std::cout << KBLU << "Target:           " << (args.operateOnFolder?string("Folder"):string("File")) << KNRM << std::endl;
    std::cout << KBLU << "Quiet:            " << (args.quiet?string("Yes"):string("No")) << KNRM << std::endl;
    std::cout << KBLU << "verifyIntegrity:  " << (args.verifyIntegrity?string("Yes"):string("No")) << KNRM << std::endl;
    std::cout << KBLU << "OScompare:        " << (args.OScompare?string("Yes"):string("No")) << KNRM << std::endl;
    std::cout << KBLU << "DemoMode:         " << (args.demoMode?string("Enabled"):string("Disabled")) << KNRM << std::endl;  
    std::cout << KBLU << "Path :            "  << args.path << KNRM << std::endl;
}

/**
 *  Display Starting Splashscreeen
 */
void show_start_splashscreen(void)
{
    std::cout << KCYN << "  ------------------------------------------------------------------------------------ " << KNRM << std::endl;
    std::cout << KCYN << " |                                                                                    |" << KNRM << std::endl;
    std::cout << KCYN << " |        GZIP FPGA Accelerator Powered by Accelize & CAST GZip IP                    |" << KNRM << std::endl;
    std::cout << KCYN << " |          Designed with QuickPlay, Protected by Algodone Embedded DRM               |" << KNRM << std::endl;
    std::cout << KCYN << " |                                                                                    |" << KNRM << std::endl;
    std::cout << KCYN << "  ------------------------------------------------------------------------------------ " << KNRM << std::endl;
    std::cout << std::endl;
}

/**
 *  Display Finish Splashscreeen
 */
void show_finish_splashscreen(void)
{
    std::cout << std::endl;
    std::cout << KCYN << " ------------------------------------------------------------------------------------" << KNRM << std::endl;
    std::cout << KCYN << " Accelerated by Accelize & CAST GZip IP (up to 26Gbps version)                       " << KNRM << std::endl;
    std::cout << KCYN << "     [Higher performance version available, please contact Accelize for more details]" << KNRM << std::endl;
    std::cout << KCYN << " Designed with QuickPlay, Protected by Algodone Embedded DRM       " << KNRM << std::endl;
    std::cout << std::endl;
}

/**
 *  Display Application Help/Usage Menu
 */
int show_usage(char* argv[])
{
    std::cerr << KBLU << "Usage: " << argv[0] << " [OPTION]... [FILE/FOLDER]..." << KNRM << std::endl;
    std::cerr << KBLU << "Compress FILEs (by default compress FILES in-place)." << KNRM << std::endl;
    std::cerr << KBLU << "" << KNRM << std::endl;
    std::cerr << KBLU << "\t-h, -? --help     give this help" << KNRM << std::endl;
    std::cerr << KBLU << "\t-q, --quiet       suppress all warnings" << KNRM << std::endl;
    std::cerr << KBLU << "\t-r, --recursive   operate recusively on directories" << KNRM << std::endl;
    std::cerr << KBLU << "\t-t, --test        test compressed/decompressed file integrity" << KNRM << std::endl;
    std::cerr << KBLU << "\t-f, --force       force overwrite of output file" << KNRM << std::endl;
    std::cerr << KBLU << "\t-v, --verbose     verbose mode" << KNRM << std::endl;
    std::cerr << KBLU << "\t-V, --version     display version number" << KNRM << std::endl;
    std::cerr << KBLU << "\t--no-compare      disable performance comparison between CPU gzip and FPGA gzip" << KNRM << std::endl;
    std::cerr << KBLU << "\t--sample-files    use gzip validation files sample (DEMO MODE)" << KNRM << std::endl;
    std::cerr << KBLU << "" << KNRM << std::endl;
    return -1;
}

/**
 *  Display QuickApps Version
 */
int show_version(void)
{
    std::cout << KCYN << "  ------------------------------------------------------------------------------------ " << KNRM << std::endl;
    std::cout << KCYN << " | QuickApp Version = " << QAPP_VERSION << "                                                           |" << KNRM << std::endl;
    std::cout << KCYN << "  ------------------------------------------------------------------------------------ " << KNRM << std::endl;
    return -1;
}

/**
 *  os_gzip_file
 */
int os_gzip_file(string comprMode, string inFile, double & elapsed, double & globBW, double & comprRatio, gzip_args_t args)
{
    if(args.verbose)
        std::cout << KBLU << "Starting GZip Software compression (" << comprMode << ") of file [" << basename(inFile) << "] " << getFileSizeStr(inFile) << " ..." << KNRM << std::endl;

    FILE *in;
    string cmd = "gzip " + comprMode + " -c " + inFile + " > /var/tmp/fakeArch.gz 2> /dev/null";

   // if files does not exist
	struct stat buf;
	if (stat(inFile.c_str(), &buf) != 0) {
        std::cerr << KRED << "Error: Unable to open file [" << inFile << "]" << KNRM << std::endl;
		return -1;
	}

    // Start Time Measurement
    chrono::time_point<std::chrono::system_clock> start = chrono::system_clock::now();

    // Create archive
    if(!(in = popen(cmd.c_str(), "r"))) {
        std::cerr << KRED << "os_gzip_file " << inFile << "failed" << KNRM << std::endl;
		return -2;
    }
    pclose(in);
    
    // Stop Time Measurement
    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    // Compute Compression ratio
    comprRatio = (double)getFileSize(inFile)/(double)getFileSize(std::string("/var/tmp/fakeArch.gz"));

    // Clean Generated File
    cmd = "rm -f /var/tmp/fakeArch.gz";
    in = popen(cmd.c_str(), "r");
    pclose(in);

    // Compute elapsed time
    globBW = getBandwidthMBps(start, end, (unsigned long)getFileSize(inFile));
    elapsed = getElapsedSecs(start, end);    

    return 0;
}

/**
 * Gzip File in FPGA
 */
int fpga_gzip_file(string in_filename, gzip_args_t args, file_results_t* res)
{ 
    double elapsed;
    double osBandwidthMBpsFast, osBandwidthMBpsBest;
    double comprRatio;
    int retCode=0;

    // Compute out_filename
    string out_filename = in_filename + string(".gz");
    res->filename = basename(in_filename);

    // Test if file already exists
    if(!args.force && isFile(out_filename)) {
        std::cerr << KRED << "File [" << out_filename << "] already exists. use '-f'/'--force' to overwrite existing files" << KNRM << std::endl;
        return -1;
    }
    
    // Compute file sizes
    infsize = getFileSize(in_filename);
    outfsizeMAX = 2*infsize;    // Compressed file could be bigger than original one

    if(args.verbose)
        std::cout << KBLU << "\nStarting GZip Hardware compression of file [" << basename(in_filename) << "] " << getFileSizeStr(in_filename) << " ..." << KNRM << std::endl;

    // Open input file
	int fin = open(in_filename.c_str(),  O_RDONLY,  S_IREAD  );
	if (fin == -1) {
        std::cerr << KRED << "fpga_gzip_file: Error: Opening input file [" << in_filename << "]" << KNRM << std::endl;
		return -1;
	}

    // Create output file
	int fout = open(out_filename.c_str(),  O_WRONLY | O_CREAT, S_IWRITE | S_IREAD  );
	if (fout == -1) {
        std::cerr << KRED << "fpga_gzip_file: Error: Opening output file [" << out_filename << "]" << KNRM << std::endl;
		return -3;
	}

	// Memory map input file
	input_file = (char *)mmap(NULL, infsize, PROT_READ, MAP_PRIVATE, fin, 0);
    if (input_file == MAP_FAILED) {
        std::cerr << KRED << "fpga_gzip_file: Memory map error on input file [" << in_filename << "] exiting..." << KNRM << std::endl;
	   return -2;
	} else
		close(fin);

    // Memory map output file (allocate)
	output_file = (char *)mmap(NULL, outfsizeMAX, PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
    if (output_file == MAP_FAILED) {
        std::cerr << KRED << "fpga_gzip_file: Memory map error on output file [" << out_filename << "] exiting..." << KNRM << std::endl;
        return -2;
	}

    // Create Related Streams
    QpStream data_in("file_in", 3);
    QpStream data_out("archive_out", 3);

#ifdef SGDMA
    // Enable stream option
    dev1.qpEnableStreamOptions(data_in);
    dev1.qpEnableStreamOptions(data_out);

    // Create and configure stream interrupt structure
    TPCIeStreamInterrupt streamIntParam;
    streamIntParam.intEnable = true;
    streamIntParam.userIrqHandler = NULL;

    /* Configure stream mode */
    data_in.setFifoSize(PCIE_FIFO_SIZE); 
    data_in.setStreamOption("streamMode", PCIE_STREAM_SGDMA);
    data_in.setStreamOption("streamIntParam", streamIntParam);
    data_out.setFifoSize(PCIE_FIFO_SIZE);
    data_out.setStreamOption("streamMode", PCIE_STREAM_SGDMA);
    data_out.setStreamOption("streamIntParam", streamIntParam);
#endif

    // Open Related Streams 
    if (dev1.qpOpenStream(data_in)) {
	    std::cerr << KRED << " => Call OpenStream failed for QpStream data_in. (size=" << (infsize>MIN_FIFO_SIZE?infsize:MIN_FIFO_SIZE) << " bytes)" << KNRM << std::endl;
	    return -1;
    }
    if (dev1.qpOpenStream(data_out)) {
	    std::cerr << KRED << " => Call OpenStream failed for QpStream data_out. (size=" << (outfsizeMAX>MIN_FIFO_SIZE?outfsizeMAX:MIN_FIFO_SIZE) << " bytes)" << KNRM << std::endl;
	    dev1.qpCloseStream(data_in);
	    return -1;
    }

    // ############################### 1rst run : 1 loop, Compression ratio comparison

    // Configure Producer, Consumer Thread
    thread_params_t prod_thread_cfg, cons_thread_cfg;
    prod_thread_cfg.pStream=&data_in;
    prod_thread_cfg.pBuffer=input_file;
    prod_thread_cfg.reqTransfSize=infsize;
    prod_thread_cfg.loopCnt=1;
    cons_thread_cfg.pStream=&data_out;
    cons_thread_cfg.pBuffer=output_file;
    cons_thread_cfg.reqTransfSize=outfsizeMAX;
    cons_thread_cfg.loopCnt=prod_thread_cfg.loopCnt;

    // Create two thread for read and write process
#ifdef SGDMA
    std::thread Consumer_thread(tConsumer_SGDMA, &cons_thread_cfg);
    std::thread Producer_thread(tProducer_SGDMA, &prod_thread_cfg);
#endif
#ifdef SGDMAR
    std::thread Consumer_thread(tConsumer_SGDMAR, &cons_thread_cfg);
    std::thread Producer_thread(tProducer_SGDMAR, &prod_thread_cfg);
#endif

    // Wait for the two thread end
    Consumer_thread.join();
	Producer_thread.join();

    // Write result to output_file
    long long int written=0;
    while(written<outfsize) {
        int ret = write(fout, &output_file[written], (outfsize-written));
        if(ret<0) {
            std::cerr << KRED << "Error: Unable to write output file [" << out_filename << "] ret=" << ret << KNRM << std::endl;
            return -4;
        }
        written += ret;
    }

    // Save Compression Result
    res->hwComprRatio = (double)infsize/(double)outfsize;


    // ############################### 2nd run : 100 loop, Bandwidth comparison
    // Configure Producer, Consumer Thread
    prod_thread_cfg.pStream=&data_in;
    prod_thread_cfg.pBuffer=input_file;
    prod_thread_cfg.reqTransfSize=infsize;
    prod_thread_cfg.loopCnt=BW_TEST_ITERATION_CNT;
    cons_thread_cfg.pStream=&data_out;
    cons_thread_cfg.pBuffer=output_file;
    cons_thread_cfg.reqTransfSize=outfsizeMAX;
    cons_thread_cfg.loopCnt=prod_thread_cfg.loopCnt;

    // Start Time Measurement
    chrono::time_point<std::chrono::system_clock> start100 = chrono::system_clock::now();

// Create two thread for read and write process
#ifdef SGDMA
    std::thread Consumer_thread100(tConsumer_SGDMA, &cons_thread_cfg);
    std::thread Producer_thread100(tProducer_SGDMA, &prod_thread_cfg);
#endif
#ifdef SGDMAR
    std::thread Consumer_thread100(tConsumer_SGDMAR, &cons_thread_cfg);
    std::thread Producer_thread100(tProducer_SGDMAR, &prod_thread_cfg);
#endif

    // Wait for the two thread end
    Consumer_thread100.join();
	Producer_thread100.join();

    // Stop Time Measurement
    std::chrono::time_point<std::chrono::system_clock> end100 = std::chrono::system_clock::now();

    // Compute Bandwidth
    res->hwBwMBps = getBandwidthMBps(start100, end100, (unsigned long)infsize*prod_thread_cfg.loopCnt);

    // ####################

    // Close the streams
    dev1.qpCloseStream(data_in);
    dev1.qpCloseStream(data_out);

	// Clear resources
	munmap(input_file, infsize);
    munmap(output_file, outfsizeMAX);
    close(fout);

    // Verify Compression Result
    if(args.verifyIntegrity)
        retCode = checkArchive(in_filename, out_filename, args.verbose);
    else
        retCode = 0;

    // Update Compression result label
    if(retCode)
        res->comprResult = std::string("FAIL");
    else
        res->comprResult = std::string("SUCCESS");

    if(args.OScompare) {
        // Launch OS Compression Process Best Compression Mode
        retCode = os_gzip_file("--best", in_filename, elapsed, osBandwidthMBpsBest, comprRatio, args);
        res->swBwBestMBps = osBandwidthMBpsBest;
        res->swComprBestRatio = comprRatio;

        // Launch OS Compression Process Fast Compression Mode
        retCode = os_gzip_file("--fast", in_filename, elapsed, osBandwidthMBpsFast, comprRatio, args);
        res->swBwFastMBps = osBandwidthMBpsFast;
        res->swComprFastRatio = comprRatio;
    
        // Compute Gains
        res->bwFastGain    = res->hwBwMBps / res->swBwFastMBps;
        res->bwBestGain    = res->hwBwMBps / res->swBwBestMBps;
        res->comprFastGain = res->hwComprRatio / res->swComprFastRatio;
        res->comprBestGain = res->hwComprRatio / res->swComprBestRatio;
    }
    else {
        res->swComprBestRatio = -1.0;
        res->swBwFastMBps = -1.0;
        res->swBwBestMBps = -1.0;
        res->swBwFastMBps = -1.0;
        res->bwFastGain = -1.0;
        res->bwBestGain = -1.0;
        res->comprFastGain = -1.0;
        res->comprBestGain = -1.0;
    }
	return 0;
}

/**
 * Gzip Folder in FPGA
 */
int fpga_gzip_folder(string folderPath, gzip_args_t args, file_results_t* & resTable, unsigned int & resTableSize)
{ 
    struct dirent **namelist;
    int n = scandir(folderPath.c_str(), &namelist, NULL, alphasort);
    resTable = new file_results_t[n];
    resTableSize=0;
    if (n > 0) {
        for(int i=0; i<n; i++) {
            string in_filepath   = folderPath + string("/") + string(namelist[i]->d_name);

            // Skip directory path files & hidden files
            if (!strncmp(namelist[i]->d_name, ".", 1))  
                continue;

            // Skip inner folders
            if(isFolder(in_filepath))
                continue;

            // Skip already existing .gz achives
            if(isGzipArchive(in_filepath))
                continue;

            // Launch GZip Compression Process
            if (fpga_gzip_file(in_filepath, args, &resTable[resTableSize]))
                return -1;
            resTableSize++;
        }

        // Clear allocated resources
        for(int i=0; i<n; i++)
            free(namelist[i]);
        free(namelist);
    }
    return 0;
}

/**
 *  clearSampleFolder
 */
int clearSampleFolder(gzip_args_t args)
{
    FILE *in;
    string cmd;

    // Clean Generated Archive Files 
    cmd = string("rm -f ") + sampleInFolderPath_gzip + string("/*.gz");
    if(!(in = popen(cmd.c_str(), "r")))
	    return -1;
    pclose(in);
    return 0;
}

/**
 *  Parse Command Line Arguments
 */
int parse_cmdline_arguments(int argc, char*argv[], gzip_args_t & args)
{
    int opt=0;    
    while( (opt= getopt(argc, argv, "h?cdqrtfvV-:"))!=-1) {
        switch(opt) {
            case 'r':   args.operateOnFolder=true; break;
            case 'q':   args.quiet=true; break;
            case 't':   args.verifyIntegrity=true; break;
            case 'f':   args.force=true; break;
            case 'v':   args.verbose=true; break;
            case 'V':   return show_version(); break;        
            case '-':   if(optarg == string("no-compare"))
                            args.OScompare=false;                 
                        if(optarg == string("sample-files"))
                            args.demoMode=true; 
                        if(optarg == string("force"))
                            args.force=true; 
                        if(optarg == string("help"))
                            return show_usage(argv);  
                        if(optarg == string("quiet"))
                            args.quiet=true;
                        if(optarg == string("recursive"))
                            args.operateOnFolder=true;
                        if(optarg == string("test"))
                            args.verifyIntegrity=true;
                        if(optarg == string("verbose"))
                            args.verbose=true;
                        if(optarg == string("version"))
                            return show_version();  
                        if(optarg == string("csv"))
                            args.writeCSV=true;                     
                        break;
            case 'h':
            case '?':            
            default:
                return show_usage(argv);
                break;
        }
    }

    /* Demo Mode */
    if(args.demoMode) {
        if(optind<argc && !args.quiet) {
            std::cout << KYEL << "WARNING: In \"sample-files\" mode, non-options argument is not required"   << KNRM << std::endl;
            std::cout << KYEL << "         Argument [" << argv[optind] << "] will be ignored"   << KNRM << std::endl;
        }
        return 0;
    }

    /* Gather non-options argument */
    if (optind >= argc) {
        if(!args.demoMode) {
            std::cerr << "Expected argument after options" << std::endl;
            return show_usage(argv);
        }
    }
    args.path=argv[optind];

    /* Verify Last Argument Validity */
    if(!args.operateOnFolder && !isFile(args.path)) {
        std::cerr << KRED << "Provided argument is not a file, please use the \"-r\" option to operate on folders " << KNRM << std::endl;
        return show_usage(argv);
    }
    if(args.operateOnFolder && !isFolder(args.path)) {
        std::cerr << KRED << "Provided argument is not a folder, you must provide a folder argument along with the \"-r\" option" << KNRM << std::endl;
        return show_usage(argv);
    }
    return 0;  
}

/**
 *  getJSONfilepath
 */
std::string getJSONfilepath(std::string rootSearchDir, std::string expectedUDID)
{
    rootSearchDir += '/';
    QpConfigInfo jsonConfigInfo;
    std::string jsonPath = std::string("");
    struct dirent **namelist;
    int n = scandir(rootSearchDir.c_str(), &namelist, NULL, alphasort);
    if (n > 0) {
        for(int i=0; i<n; i++) {
            string path  = rootSearchDir + string("/") + string(namelist[i]->d_name);

            // Skip directory path files & hidden files
            if (!strncmp(namelist[i]->d_name, ".", 1))  
                continue;

            // Inner folder treated as recursive calls
            if(isFolder(path))
                jsonPath = getJSONfilepath(path, expectedUDID);
            // Test each Json file
            else if(isJsonFile(path)) {
                jsonConfigInfo.parsingINIfile(basename(path).substr(0,basename(path).size()-5), rootSearchDir);
                if(expectedUDID == jsonConfigInfo.getUdid())
                    jsonPath = rootSearchDir;  
            }

            if(jsonPath != "")
                break;
        }
        // Clear allocated resources
        for(int i=0; i<n; i++)
            free(namelist[i]);
        free(namelist);
    }
    return jsonPath;
}

/**
 *  getDesignUDID
 */
std::string getDesignUDID(bool verbose)
{
    TPCIeConnHdl pcieConnectionHandler=NULL;
    TPCIeParam   pcieConnectionParams;
    pcieConnectionParams.VendorID   = 0x1556;
    pcieConnectionParams.DeviceID   = 0x2000;
    pcieConnectionParams.BoardIndex = 0x00;     //TODO: Handle multi-board when available
    UINT32 regValue[4];
    std::string designUDID="";

    // Read UDID Values in the design
    if(QuickAPI_ConnectDevice(pcieConnectionParams, pcieConnectionHandler)) {
        std::cerr << KRED << "Unable to connect to a QuickPlay design. Please load a bitstream into the board" << KNRM << std::endl;
        return designUDID;
    }
    if(QuickAPI_ReadRegister(pcieConnectionHandler, 0, regValue, 4)) {
        std::cerr << KRED << "Unable to read design UDID" << KNRM << std::endl;
        return designUDID;
    }
    QuickAPI_DisconnectDevice(pcieConnectionHandler);

    // Convert int[4] to UDID hex string
    std::stringstream stream;
    stream << std::hex;
    stream << std::setfill ('0') << std::setw(8) << (regValue[3])        << '-';
    stream << std::setfill ('0') << std::setw(4) << (regValue[2]>>16)    << '-';
    stream << std::setfill ('0') << std::setw(4) << (regValue[2]&0xFFFF) << '-';
    stream << std::setfill ('0') << std::setw(4) << (regValue[1]>>16)    << '-';
    stream << std::setfill ('0') << std::setw(4) << (regValue[1]&0xFFFF);
    stream << std::setfill ('0') << std::setw(8) << regValue[0];
    designUDID = stream.str();

    if(verbose)
        std::cout << KBLU << "Design UDID = [" <<  designUDID << "]" << KNRM << std::endl;

    return designUDID;
}

/**
 *  Entry Point
 */
int main(int argc, char*argv[])
{    
	int     retCode=0;
    gzip_args_t args;
    args.operateOnFolder=false; // working on files by default
    args.quiet=false;           // print all infos by default
    args.verifyIntegrity=false; // don't check file integrity by default
    args.OScompare=true;        // compare with OS GZip by default
    args.demoMode=false;        // Not in demo mode by default
    args.verbose=false;         // No verbosity by default
    args.force=false;           // No overwrite output file by default
    args.writeCSV=false;        // No csv output by default

    // Display Startup Splashscreen
    show_start_splashscreen();

    /* Parse Arguments */
    if( (retCode=parse_cmdline_arguments(argc, argv, args)) !=0 )
        return retCode;

    /* Get Design UDID */   
    std::string designUDID = getDesignUDID(args.verbose);

    /* Retrieve json_file_path from design */
    string jsonPath = getJSONfilepath(std::string(JSON_SEARCH_PATH), designUDID);
    if(jsonPath=="") {
        std::cerr << KRED << "Unable to find JSON file matching loaded design UDID [" << designUDID << "]" << KNRM << std::endl;
        return -1;
    }
    if(args.verbose)
        std::cerr << KBLU << "Using JSON file from [" << jsonPath << '/' << string(DEVICE_NAME) << ".json]"  << KNRM << std::endl;
    
    /* Open the device */
	if (dev1.qpOpenDesign(DEVICE_NAME, LIC_SEARCH_PATH, jsonPath.c_str()))
		return -1;

    /* Reset Design Internal Components */
    dev1.qpResetDesign();
    
	/* Start HwLogger Thread */
    std::thread HwLogger_thread(tHwLogger);

    /* Create ResTable data */
    file_results_t* pResTable;
    unsigned int resTableSize=0;

    /* Launch GZip Compression Process */ 
    if(args.demoMode) {
        retCode = fpga_gzip_folder(sampleInFolderPath_gzip, args, pResTable, resTableSize);
        clearSampleFolder(args);
    }
    else {
        if(args.operateOnFolder)
            retCode = fpga_gzip_folder(args.path, args, pResTable, resTableSize);
        else {
            pResTable = new file_results_t[1];
            resTableSize=1;
            retCode = fpga_gzip_file(args.path, args, pResTable);
        }
    }

    /* Print Result Table & Save Results in CSV file */
    if (!retCode) {
        display_result_table(pResTable, resTableSize);
        if(args.writeCSV)
			save_result_table_csvfile(pResTable, resTableSize);
	}
    delete[] pResTable;
    
    /* Terminate the logger thread */
    hwLoggerExit = true;
    HwLogger_thread.join();

	/* Close the device */
	if ( dev1.qpCloseDesign() )
		return -1;

    // Display Exit Splashscreen
    show_finish_splashscreen();

	return 0;
}
