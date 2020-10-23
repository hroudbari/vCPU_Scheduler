#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>

struct _domainStats {
	int id;
	//char domName[8];
	unsigned long long deltaVCPUcycles;
	bool isPinned;
};

typedef struct _domainStats domainStats;
typedef domainStats * domainStatsPtr;

void computeVcputoPcpuMapping (domainStatsPtr **inTable, int numPcpus,int numDomains, int busiestPCPU, int freeiestPCPU, int time_int, int *domainIDs, virConnectPtr conn);

int main (int argc, char *argv[]) {
	
	if(argc != 2)
	{
		fprintf(stderr, "Please provide one argument for time interval! Exiting.. \n");
		exit(1);
	}
	unsigned int time_int = atoi(argv[1]);
	
	virConnectPtr conn;
	conn = virConnectOpen("qemu:///system");

	if (conn == NULL) 
	{
		fprintf(stderr, "Failed to open connection to qemu://system\n");
		exit(1);
	}

	virNodeInfoPtr info = (virNodeInfoPtr) malloc(sizeof(virNodeInfo));
	int statusNodeInfo = virNodeGetInfo (conn, info);
	int numPCPUs = 0;
	if(statusNodeInfo == 0) 
	{
		numPCPUs = info->cpus;//info->cores * info->sockets * info->nodes;
		//printf("Found %d pcpus \n", numPCPUs);
	}
	else 
	{
		printf("Failed to get nodeInfo. Exiting ... \n");
		exit(1);
	}

	/*
	char *host;
	host = virConnectGetHostname(conn);
	fprintf(stdout, "Hostname: %s\n", host);
	free(host);

	int vcpus;
	vcpus = virConnectGetMaxVcpus(conn, NULL);
	printf("Maximum support virtual CPUs: %d\n", vcpus);
	*/	

	//***************************************************initialize data structures

	int numDomains = virConnectNumOfDomains(conn);
	if(numDomains == -1) 
	{
		printf("Failed to get number of domains. Exiting ... \n");
		exit(1);
	}
	//else printf("Found %d domains \n", numDomains);

	int *domainIDs = malloc(numDomains*sizeof(int));
	int domains = virConnectListDomains (conn, domainIDs, numDomains);
	//printf("Domains = %d \n", domains); 

	//for(int i = 0; i < numDomains ; i++) printf("domainIDs[%d] = %d \n",i,domainIDs[i]);

	//allocating memory for mapping of pcpu vs. domain (inTable structure)
	domainStatsPtr **inTable = (domainStatsPtr **) malloc( numPCPUs * sizeof(domainStatsPtr *));
	for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
	{
		inTable[pcpu] = (domainStatsPtr *) malloc( domains * sizeof(domainStatsPtr));

		for(int i=0; i<numDomains; i++)
		{
			inTable[pcpu][i] = malloc(sizeof(domainStats));
			memset(inTable[pcpu][i],0,sizeof(domainStats));
		}
	}

	//populating inTable structure
	unsigned char *cpumaps = calloc(1,1);	
	memset(cpumaps,0,sizeof(char));
	//virTypedParameterPtr params;
	for(int i = 0; i < numDomains ; i++)	
	{
		virDomainPtr dom = virDomainLookupByID(conn,domainIDs[i]);
		for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
		{			
			virDomainGetVcpuPinInfo(dom,1,cpumaps,1,VIR_DOMAIN_AFFECT_CURRENT);
			inTable[pcpu][i]->id = domainIDs[i];
			unsigned char mask = 0x01 << pcpu;
			inTable[pcpu][i]->isPinned = (*cpumaps & mask) >> pcpu;
			//printf("inTable[%d][%d] : id = %d      isPinned = %d    \n", pcpu,i,inTable[pcpu][i]->id,inTable[pcpu][i]->isPinned);
		}
	}

	//initializing prevVCPUCycles and currVCPUCycles array data structures
	unsigned long long prevVCPUCycles [numPCPUs][numDomains];
	unsigned long long currVCPUCycles [numPCPUs][numDomains];

	
	//initializing prevPCPUCycles and currPCPUCycles and deltaPCPUCycles array data structures
	unsigned long long prevPCPUTotalCycles [numPCPUs];
	unsigned long long currPCPUTotalCycles [numPCPUs];
	unsigned long long deltaPCPUTotalCycles [numPCPUs];

	//initializing the data structures 
	for(int i = 0; i < numPCPUs ; i++)
	{
		prevPCPUTotalCycles [i] = 0;
		currPCPUTotalCycles [i] = 0;
		deltaPCPUTotalCycles [i] = 0;
		for(int j= 0; j< numDomains; j++)
		{
			prevVCPUCycles[i][j] = 0;
			currVCPUCycles[i][j] = 0;
		}
	}
	//***************************************************end of initialize data structures

	unsigned int currNumDomains;
	unsigned int prevNumDomains = virConnectNumOfDomains(conn);

	while(virConnectNumOfDomains(conn)>0)
	{
		currNumDomains = virConnectNumOfDomains(conn);
		if(currNumDomains != prevNumDomains)
		{
			if(currNumDomains != prevNumDomains)
			{
				if(domainIDs!=NULL) free(domainIDs);
				if(**inTable!=NULL) free(**inTable);
				if(prevVCPUCycles!=NULL) free(prevVCPUCycles);
				if(currVCPUCycles!=NULL) free(currVCPUCycles);
			}
			
			int numDomains = virConnectNumOfDomains(conn);
			if(numDomains == -1) 
			{
				printf("Failed to get number of domains. Exiting ... \n");
				exit(1);
			}
			//else printf("Found %d domains \n", numDomains);

			int *domainIDs = malloc(numDomains*sizeof(int));
			int domains = virConnectListDomains (conn, domainIDs, numDomains);
			//printf("Domains = %d \n", domains); 

			//for(int i = 0; i < numDomains ; i++) printf("domainIDs[%d] = %d \n",i,domainIDs[i]);

			//allocating memory for mapping of pcpu vs. domain (inTable structure)
			domainStatsPtr **inTable = (domainStatsPtr **) malloc( numPCPUs * sizeof(domainStatsPtr *));
			for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
			{
				inTable[pcpu] = (domainStatsPtr *) malloc( domains * sizeof(domainStatsPtr));

				for(int i=0; i<numDomains; i++)
				{
					inTable[pcpu][i] = malloc(sizeof(domainStats));
					memset(inTable[pcpu][i],0,sizeof(domainStats));
				}
			}

			//populating inTable structure
			unsigned char *cpumaps = calloc(1,1);	
			memset(cpumaps,0,sizeof(char));
			//virTypedParameterPtr params;
			for(int i = 0; i < numDomains ; i++)	
			{
				virDomainPtr dom = virDomainLookupByID(conn,domainIDs[i]);
				for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
				{			
					virDomainGetVcpuPinInfo(dom,1,cpumaps,1,VIR_DOMAIN_AFFECT_CURRENT);
					inTable[pcpu][i]->id = domainIDs[i];
					unsigned char mask = 0x01 << pcpu;
					inTable[pcpu][i]->isPinned = (*cpumaps & mask) >> pcpu;
					//printf("inTable[%d][%d] : id = %d      isPinned = %d    \n", pcpu,i,inTable[pcpu][i]->id,inTable[pcpu][i]->isPinned);
				}
			}

			//initializing prevVCPUCycles and currVCPUCycles array data structures
			unsigned long long prevVCPUCycles [numPCPUs][numDomains];
			unsigned long long currVCPUCycles [numPCPUs][numDomains];

			
			//initializing prevPCPUCycles and currPCPUCycles and deltaPCPUCycles array data structures
			unsigned long long prevPCPUTotalCycles [numPCPUs];
			unsigned long long currPCPUTotalCycles [numPCPUs];
			//unsigned long long deltaPCPUTotalCycles [numPCPUs];

			//initializing the data structures 
			for(int i = 0; i < numPCPUs ; i++)
			{
				prevPCPUTotalCycles [i] = 0;
				currPCPUTotalCycles [i] = 0;
				deltaPCPUTotalCycles [i] = 0;
				for(int j= 0; j< numDomains; j++)
				{
					prevVCPUCycles[i][j] = 0;
					currVCPUCycles[i][j] = 0;
				}
			}
			
			//copying currCycles to prevCycles
			memset(&prevVCPUCycles,0,numPCPUs*numDomains*sizeof(unsigned long long));
			memcpy(prevVCPUCycles, currVCPUCycles, numPCPUs*numDomains*sizeof(unsigned long long));

			memset(&prevPCPUTotalCycles,0,numPCPUs*sizeof(unsigned long long));
			memcpy(prevPCPUTotalCycles,currPCPUTotalCycles,numPCPUs*sizeof(unsigned long long));

			
			virTypedParameterPtr params;			
			for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
			{
				for(int i = 0; i < numDomains ; i++)	
				{
					virDomainPtr dom = virDomainLookupByID(conn,domainIDs[i]);
							
					int nparams = virDomainGetCPUStats(dom, NULL, 0, pcpu, 1, 0);
					if(nparams>0)
					{
						// Allocate memory to hold the return contents
						params = calloc(1 * nparams, sizeof(virTypedParameter));
						virDomainGetCPUStats(dom, params, nparams, pcpu, 1, 0);
						currVCPUCycles[pcpu][i] = params[1].value.ul;
						currPCPUTotalCycles[pcpu] += params[0].value.ul;
					 	free(params);
					}	
				}
			}

			for(int i = 0; i < numDomains ; i++)	
			{
				for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
				{			
					inTable[pcpu][i]->deltaVCPUcycles = currVCPUCycles[pcpu][i] - prevVCPUCycles[pcpu][i] ;

					//printf("inTable[%d][%d] : cycles = %lld  id = %d  isPinned = %d\n", pcpu,i,inTable[pcpu][i]->deltaVCPUcycles,inTable[pcpu][i]->id,inTable[pcpu][i]->isPinned);
				}
			}

			printf("\n\n\n\n");
		}
		else
		{
			
			//copying currCycles to prevCycles
			memset(&prevVCPUCycles,0,numPCPUs*numDomains*sizeof(unsigned long long));
			memcpy(prevVCPUCycles, currVCPUCycles, numPCPUs*numDomains*sizeof(unsigned long long));

			memset(&prevPCPUTotalCycles,0,numPCPUs*sizeof(unsigned long long));
			memcpy(prevPCPUTotalCycles,currPCPUTotalCycles,numPCPUs*sizeof(unsigned long long));

			virTypedParameterPtr params;
			for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
			{
				currPCPUTotalCycles[pcpu] = 0;
				for(int i = 0; i < numDomains ; i++)	
				{
					virDomainPtr dom = virDomainLookupByID(conn,domainIDs[i]);
							
					int nparams = virDomainGetCPUStats(dom, NULL, 0, pcpu, 1, 0);
					if(nparams>0)
					{
						// Allocate memory to hold the return contents
						params = calloc(1 * nparams, sizeof(virTypedParameter));
						virDomainGetCPUStats(dom, params, nparams, pcpu, 1, 0);
						currVCPUCycles[pcpu][i] = params[1].value.ul;
						
						currPCPUTotalCycles[pcpu] += params[0].value.ul;
					 	free(params);
					}	
				}
			}

			for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
			{
				//finding deltaPCPUTotalCycles
				deltaPCPUTotalCycles [pcpu] = currPCPUTotalCycles [pcpu] - prevPCPUTotalCycles [pcpu];
				//printf("CPU %d : deltaPCPUTotalCycles = %lld , currPCPUTotalCycles =  %lld , prevPCPUTotalCycles = %lld  \n", pcpu, deltaPCPUTotalCycles [pcpu], currPCPUTotalCycles [pcpu], prevPCPUTotalCycles [pcpu]);
				for(int i = 0; i < numDomains ; i++)	
				{			
					inTable[pcpu][i]->deltaVCPUcycles = currVCPUCycles[pcpu][i] - prevVCPUCycles[pcpu][i] ;
					//printf("inTable[%d][%d] : id = %d  isPinned = %d cycles = %lld  \n", pcpu,i,inTable[pcpu][i]->id,inTable[pcpu][i]->isPinned, inTable[pcpu][i]->deltaVCPUcycles);
				}
			}
			//printf("\n\n\n\n");
			
			//Finding busiest and free-est PCPU

			double minPCPUPercentUtil  = 1000000000.0;
			double maxPCPUPercentUtil = 0.0;
			double pCPUUtilPercent = 0.0;

			// Find the most occupied and most free cpu.
			int busiestPcpu, freeiestPcpu;

			for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
			{
				pCPUUtilPercent = ((double)deltaPCPUTotalCycles[pcpu]/(time_int*1e9))*100.0;
				//printf("CPU %d Util: %f percent \t deltaPCPUTotalCycles = %lld \n", pcpu,pCPUUtilPercent, deltaPCPUTotalCycles[pcpu]);
				//printf("CPU %d Util: %0.2f %% \n", pcpu,pCPUUtilPercent);
				// Find the busiest and freeiest pCpus.
				if(pCPUUtilPercent < minPCPUPercentUtil){
					freeiestPcpu = pcpu;
					minPCPUPercentUtil = pCPUUtilPercent;
				}

				if(pCPUUtilPercent > maxPCPUPercentUtil){
					busiestPcpu = pcpu;
					maxPCPUPercentUtil = pCPUUtilPercent;
				}
			}
			//printf("Busiest pcpu = %d, Free-est pcpu = %d  \n", busiestPcpu, freeiestPcpu);
			//printf("maxPCPUPercentUtil = %f , minPCPUPercentUtil = %f \n", maxPCPUPercentUtil, minPCPUPercentUtil); 
			if( (maxPCPUPercentUtil - minPCPUPercentUtil) < 10)
			{
				//printf("Diff util perc is < 10 %% \n\n");			
			}
			else 
			{
				//proceed to calculate new pinnings.
				//printf("Diff util perc is > 10 %% \n\n");
				//printf("Debug 2: \n"); 
				for(int dom=0; dom<numDomains; ++dom)
				{
					for(int pcpu=0; pcpu<numPCPUs; ++pcpu)
					{			
						//printf("intable[%d][%d] -> deltaVCPUcycles = %lld \n", pcpu, dom, inTable[pcpu][dom] -> deltaVCPUcycles);
					}
				}
								
				computeVcputoPcpuMapping (inTable, numPCPUs, numDomains, busiestPcpu, freeiestPcpu, time_int, domainIDs, conn);

			}
			
		}
		prevNumDomains = currNumDomains;
		sleep(time_int);
	}
	return 1;

}


void computeVcputoPcpuMapping (domainStatsPtr **inTable, int numPcpus,int numDomains, int busiestPCPU, int freeiestPCPU, int time_int, int *domainIDs, virConnectPtr conn)
{	
	//Calculate total per PCPU utilization
	//unsigned long long totalPcpuUtil[numPcpus];
	//unsigned long long  allPcpuCycles;
	
	unsigned long long busiestVCPUCycles = 0;
	unsigned long long freeiestVPCUCycles = 0;
	double busiestVCPUPerc = 0.0;
	double freeiestVPCUPerc = 0.0;

	for(int dom=0; dom<numDomains; ++dom)
	{
		//if(inTable[busiestPCPU][dom]->isPinned == true) 
		//{
			busiestVCPUCycles += inTable[busiestPCPU][dom]->deltaVCPUcycles;
			//printf("Busiest Delta VCPU Cycles %lld \n\n",inTable[busiestPCPU][dom]->deltaVCPUcycles);
			//printf("inside function!!!!");
		//}
		//if(inTable[freeiestPCPU][dom]->isPinned == true) 
			freeiestVPCUCycles += inTable[freeiestPCPU][dom]->deltaVCPUcycles;
	}

	//printf("busiestVCPUCycles = %lld  \n",busiestVCPUCycles);
	//printf("freeiestVPCUCycles = %lld \n",freeiestVPCUCycles);

	busiestVCPUPerc = 100* (busiestVCPUCycles/(time_int * 1e9));
	freeiestVPCUPerc = 100*(freeiestVPCUCycles/(time_int * 1e9));

	//printf("busiestPCPU = %d  busiestVCPUPerc = %f  \n",busiestPCPU, busiestVCPUPerc);
	//printf("freeiestPCPU = %d  freeiestVPCUPerc = %f \n",freeiestPCPU,freeiestVPCUPerc);

	//printf("busiestPCPU = %d  \n",busiestPCPU);
	//printf("freeiestPCPU = %d \n",freeiestPCPU);

	

	double threshold = (busiestVCPUPerc - freeiestVPCUPerc)/2.0;
	
	double smallestDifference = 100000.0;
	int domWithSmallestDifference = -1;
	for(int dom=0; dom<numDomains; ++dom)
	{
		double tempDifference = fabs(100.0*(inTable[busiestPCPU][dom]->deltaVCPUcycles/(time_int * 1e9)) - threshold);
		if(tempDifference < smallestDifference) 
		{
			smallestDifference = tempDifference;
			domWithSmallestDifference = dom;
		}	
	}

	//printf("domWithSmallestDifference = %d \n",domWithSmallestDifference);
	//inTable[busiestPCPU][domWithSmallestDifference]->isPinned = false;
	//inTable[freeiestPCPU][domWithSmallestDifference]->isPinned = true;

	//unsigned char pinMapMaskBusiest = (~((char)1 << busiestPCPU));
	unsigned char pinMapMaskFreeiest = ((int)1 << freeiestPCPU);
						
	unsigned char *cpumaps = calloc(1,1);	
	memset(cpumaps,0,sizeof(char));

	virDomainPtr domain = virDomainLookupByID(conn,domainIDs[domWithSmallestDifference]); 
	virDomainGetVcpuPinInfo(domain,1,cpumaps,1,VIR_DOMAIN_AFFECT_CURRENT);
	unsigned char pinmap = pinMapMaskFreeiest;//(*cpumaps & pinMapMaskBusiest)| pinMapMaskFreeiest;
	virDomainPinVcpu(domain,0,&pinmap,1);

	//printing out pinning info

	for(int dom=0; dom<numDomains; ++dom)
	{
		virDomainPtr domain = virDomainLookupByID(conn,domainIDs[dom]); 
		virDomainGetVcpuPinInfo(domain,1,cpumaps,1,VIR_DOMAIN_AFFECT_CURRENT);
		//printf("Dom%d, CPU maps : %x\n",dom,*cpumaps);
	}

	return;
}



