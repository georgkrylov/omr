#ifndef ELFLOADER_HPP
#define ELFLOADER_HPP

#if defined(LINUX)

#include <elf.h>
#include <stdio.h>

class ELFLoader
{
public:
  ELFLoader(char *elfFileName);

  ~ELFLoader();
  void *getTextSection();
  Elf64_Sym *getSymbolTable();
  
protected:
  typedef Elf64_Ehdr ELFEHeader;
  typedef Elf64_Shdr ELFSectionHeader;
  typedef Elf64_Phdr ELFProgramHeader;
  typedef Elf64_Addr ELFAddress;
  typedef Elf64_Sym  ELFSymbol;
  typedef Elf64_Rela ELFRela;
  typedef Elf64_Off  ELFOffset;
#define ELF_ST_INFO(bind, type) ELF64_ST_INFO(bind,type)
#define ELF_ST_VISIBILITY(visibility) ELF64_ST_VISIBILITY(visibility)
#define ELF_R_INFO(bind, type) ELF64_R_INFO(bind, type)
#define ELFClass ELFCLASS64;

  char       *_elfFileName;
  FILE       *_elfFile;
  ELFEHeader *_header;
  
  ELFSectionHeader *_zeroSection;
  char              _zeroSectionName[1];
  ELFSectionHeader *_textSection;
  char              _textSectionName[6];
  ELFSectionHeader *_relaSection;
  char              _relaSectionName[11];
  ELFSectionHeader *_dynSymSection;
  char              _dynSymSectionName[8];
  ELFSectionHeader *_shStrTabSection;
  char              _shStrTabSectionName[10];
  ELFSectionHeader *_dynStrSection;
  char              _dynStrSectionName[8];

  void *_text;
  Elf64_Sym *_symtab;
  char *_dynstr;
  Elf64_Rela *_rela;

  void initialize();
  void loadTextSection();
  void loadSymTab();
  void loadDynStr();
  void loadRela();
  
}; //class ELFLoader

#endif //LINUX
#endif //ifndef ELFLOADER_HPP
