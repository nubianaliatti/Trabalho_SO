/*
*  myfs.c - Implementacao do sistema de arquivos MyFS
*
*  Autores: Gabriel Maciel Furlong - 201965204AB
            Pablo Mendes Gal de Castro - 202076013
            henrique barral - 202035029
            Nubia Ribeiro Naliatti - 202035007
*  Projeto: Trabalho Pratico II - Sistemas Operacionais
*  Organizacao: Universidade Federal de Juiz de Fora
*  Departamento: Dep. Ciencia da Computacao
*
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> 
#include "myfs.h"
#include "vfs.h"
#include "inode.h"
#include "util.h"

//Declaracoes globais
//...
//...
typedef struct
{
	int fd;
	Inode *inode;
	int blocksize;
	int lastByteRead;
	const char *path;
	Disk *disk;
} Arquivo;

Arquivo *arquivos [MAX_FDS] = {NULL};

//Funcao para verificacao se o sistema de arquivos está ocioso, ou seja,
//se nao ha quisquer descritores de arquivos em uso atualmente. Retorna
//um positivo se ocioso ou, caso contrario, 0.
int myFSIsIdle (Disk *d) {

	for (int i = 0; i < MAX_FDS; i++){
		if (arquivos[i] != NULL)
			return 0;
	}
	return 1;
}

//Funcao para formatacao de um disco com o novo sistema de arquivos
//com tamanho de blocos igual a blockSize. Retorna o numero total de
//blocos disponiveis no disco, se formatado com sucesso. Caso contrario,
//retorna -1.
int myFSFormat (Disk *d, unsigned int blockSize) {

    //Define a quantidade de setores a serem criados inodes 
    unsigned int numeroInode = blockSize/512;  
    int offset = inodeAreaBeginSector();
    int localInode = numeroInode + offset;
    //Cria inodes
    for (unsigned int i = offset; i <= localInode; ++i) {
		// Cria inode apos o inicial
        Inode *inode = inodeCreate(i+1, d);
        inodeSave(inode);
    }

    //printa o trecho de inodes criados 
    printf("\nInode criado de %u a %u \n", inodeAreaBeginSector(), (numeroInode + inodeAreaBeginSector()));

	// Define o primeiro setor disponivel para criar o Inode
    // Que consiste no BlockSize in inserido pelo usuário + os setores offset 
    unsigned int primeiroDisponivel = inodeAreaBeginSector() + numeroInode;

	// Auxiliar para contar a quantidade de blocos livres:
    unsigned long blocosLivres = 0;

    //Função para calcular  a quantidade de blocos livres 
    //definidas pelo grupo para serem uasadas na formatação 
    // igual a 4x a quantidade de Inodes criada 
    int qtdeBlocos = (4*blockSize)/512;

    //Variavel auxiliar para pegar a quantidade de setores a serem formatados 
    //de acordo com a função acima
    int rangeBlocos = qtdeBlocos + primeiroDisponivel;

	// Formata os blocos da área livre
    unsigned char formata[blockSize];
    memset(formata, 0, sizeof(formata));
    for (unsigned int i = primeiroDisponivel; i <= rangeBlocos; ++i) {
		// Caso não consiga escrever no setor
        if (diskWriteSector(d, i, formata) != 0) {
            return -1; 
        }
		blocosLivres++; 
    }

	

	// Define o primeiro Inode após o offset como a raiz
	unsigned int numeroRaiz = inodeFindFreeInode(inodeAreaBeginSector(), d);

    // Carregar o i-node do diretório raiz
    Inode *rootInode = inodeLoad(numeroRaiz, d);


    // Configura o i-node como raiz
    inodeSetFileType(rootInode, FILETYPE_DIR);
    inodeSetFileSize(rootInode, 0);
    inodeSetOwner(rootInode, 1);
    inodeSetRefCount(rootInode, 1);


    //printa a quantidade de blocos livres formatados
	printf ("\nblocos livres formatados: %lu \n", blocosLivres);
    return blocosLivres;
}
//Funcao para abertura de um arquivo, a partir do caminho especificado
//em path, no disco montado especificado em d, no modo Read/Write,
//criando o arquivo se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpen (Disk *d, const char *path) {
    
    Arquivo *a = NULL;
    for (int i = 0; i < MAX_FDS; i++) {
        if (arquivos[i] != NULL && strcmp(arquivos[i]->path, path) == 0)
            a = arquivos[i];
    }

    if (a == NULL) {
        a = malloc(sizeof(Arquivo));
        a->path = path;
        Inode *inode = NULL;
        int num = -1;
        for (int i = 0; i < MAX_FDS; i++) {
            if (arquivos[i] == NULL) {
                num = inodeFindFreeInode(1, d);
                inode = inodeCreate(num, d);
                break;
            }
        }

        if (num == -1)
            return -1;

        a->inode = inode;
        a->disk = d;
        a->fd = inodeGetNumber(inode);
		a->lastByteRead = 0;
        a->blocksize = 512;  

        arquivos[a->fd - 1] = a;
        return a->fd;
    }
    return -1;
}
	
//Funcao para a leitura de um arquivo, a partir de um descritor de
//arquivo existente. Os dados lidos sao copiados para buf e terao
//tamanho maximo de nbytes. Retorna o numero de bytes efetivamente
//lidos em caso de sucesso ou -1, caso contrario.
int myFSRead (int fd, char *buf, unsigned int nbytes) {
	return -1;
}

//Funcao para a escrita de um arquivo, a partir de um descritor de
//arquivo existente. Os dados de buf serao copiados para o disco e
//terao tamanho maximo de nbytes. Retorna o numero de bytes
//efetivamente escritos em caso de sucesso ou -1, caso contrario
// Função para escrever dados em um arquivo no sistema de arquivos MyFS
#define MYFS_MAX_FILES 128  // Número máximo de arquivos suportados
typedef struct {
    char filename[MAX_FILENAME_LENGTH + 1];
    unsigned int inodeNumber;
    int isOpen;
    unsigned int readPointer;
    unsigned int writePointer;
} MyFSFile;

typedef struct {
    char dirname[MAX_FILENAME_LENGTH + 1];
    unsigned int inodeNumber;
    int isOpen;
    unsigned int readPointer;
} MyFSDirectory;

typedef struct {
    char magicNumber[4];
    unsigned int blockSize;
    MyFSFile files[MYFS_MAX_FILES];
    MyFSDirectory directories[MYFS_MAX_FILES];
} MyFSData;

static MyFSData myFSData;


int myFSWrite(int fd, const char *buf, unsigned int nbytes) {

    //Verifica se possui erros 
    if (fd <= 0 || fd > MAX_FDS || arquivos[fd] == NULL) {
        return -1; 
    }
    
    Arquivo *arquivo = arquivos[fd];
    Inode *inode = arquivo->inode;
    Disk *d = arquivo->disk;
    unsigned int blocksize = arquivo->blocksize;

    //define em qual setor vai ser escrito  
     int currentBlock = arquivo->lastByteRead / blocksize;
    //Verifica se tá no inicio de um bloco se não tiver vai pro inicio do proximo 
     int startBlock = arquivo->lastByteRead % blocksize == 0 ? currentBlock : currentBlock + 1;

    //pega o número do Inode  
    int inodeNumber = inodeGetNumber(inode);

    //variavel auxiliar para registrar a quantidade de bits lidos 
    int bytesWritten = 0;

    //Pega o endereço do bloco de Inodes
    for (int block = startBlock; block <= currentBlock + nbytes / blocksize; ++block) {
        
        unsigned int blockAddr = inodeGetBlockAddr(inode, block);
        if (blockAddr == 0) {
            
            blockAddr = inodeAddBlock(inode, block);
            if (blockAddr == -1) {
                
                return -1;
            }
        }
        //define a quantidade de bytes offset utilizados 
        int offset = inodeAreaBeginSector();
        
        //Le o setor para verificar se possui alguma variável diferente de zero 
        unsigned char blockData[blocksize];
        if (diskReadSector(d, blockAddr, blockData) != 0) {
            return -1;
        }
        
        for (int i = offset; i < blocksize && bytesWritten < nbytes; ++i) {
            blockData[i] = buf[bytesWritten++];
        }
        //realiza a escrita dos byttes enquanto o byte não for 
        //difente de 0 ou seja encerra caso já tenha algo escrito 
        if (diskWriteSector(d, blockAddr, blockData) != 0) {
            return -1;
        }
        else{
            arquivo->lastByteRead = nbytes;
        }
    }
    
    unsigned int fileSize = inodeGetFileSize(inode);
    if (arquivo->lastByteRead > fileSize) {
        inodeSetFileSize(inode, arquivo->lastByteRead);
    }

    if (inodeSave(inode) != 0) {
        return -1;
    }

    return bytesWritten;
}

//Funcao para fechar um arquivo, a partir de um descritor de arquivo
//existente. Retorna 0 caso bem sucedido, ou -1 caso contrario
int myFSClose (int fd) {
	if (fd > 0 || fd <= MAX_FDS){

		Arquivo *a = arquivos[fd-1];

		arquivos[fd - 1] = NULL;
		free(a->inode);
		free(a);

		return 0;
	}
	return -1;
}

//Funcao para abertura de um diretorio, a partir do caminho
//especificado em path, no disco indicado por d, no modo Read/Write,
//criando o diretorio se nao existir. Retorna um descritor de arquivo,
//em caso de sucesso. Retorna -1, caso contrario.
int myFSOpenDir (Disk *d, const char *path) {
	return -1;
}

//Funcao para a leitura de um diretorio, identificado por um descritor
//de arquivo existente. Os dados lidos correspondem a uma entrada de
//diretorio na posicao atual do cursor no diretorio. O nome da entrada
//e' copiado para filename, como uma string terminada em \0 (max 255+1).
//O numero do inode correspondente 'a entrada e' copiado para inumber.
//Retorna 1 se uma entrada foi lida, 0 se fim de diretorio ou -1 caso
//mal sucedido
int myFSReadDir (int fd, char *filename, unsigned int *inumber) {
	return -1;
}

//Funcao para adicionar uma entrada a um diretorio, identificado por um
//descritor de arquivo existente. A nova entrada tera' o nome indicado
//por filename e apontara' para o numero de i-node indicado por inumber.
//Retorna 0 caso bem sucedido, ou -1 caso contrario.
int myFSLink (int fd, const char *filename, unsigned int inumber) {
	return -1;
}

//Funcao para remover uma entrada existente em um diretorio, 
//identificado por um descritor de arquivo existente. A entrada e'
//identificada pelo nome indicado em filename. Retorna 0 caso bem
//sucedido, ou -1 caso contrario.
int myFSUnlink (int fd, const char *filename) {
	return -1;
}

//Funcao para fechar um diretorio, identificado por um descritor de
//arquivo existente. Retorna 0 caso bem sucedido, ou -1 caso contrario.	
int myFSCloseDir (int fd) {
	return -1;
}

//Funcao para instalar seu sistema de arquivos no S.O., registrando-o junto
//ao virtual FS (vfs). Retorna um identificador unico (slot), caso
//o sistema de arquivos tenha sido registrado com sucesso.
//Caso contrario, retorna -1
int installMyFS (void) {	
    FSInfo *fs = malloc(sizeof(FSInfo));
	fs->fsid = '1';
	fs->fsname = "Trabalho_SO";
	fs->isidleFn = myFSIsIdle;
	fs->formatFn = myFSFormat;
	fs->openFn = myFSOpen;
	fs->readFn = myFSRead;
	fs->writeFn = myFSWrite;
	fs->closeFn = myFSClose;
	fs->opendirFn = myFSOpenDir;
	fs->readdirFn = myFSReadDir;
	fs->linkFn = myFSLink;
	fs->unlinkFn = myFSUnlink;
	fs->closedirFn = myFSCloseDir;
	vfsInit();
	vfsRegisterFS(fs);
	return -1;
}
