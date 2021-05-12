#include "ELFLoader.hpp"

#if defined(LINUX)

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

ELFLoader::ELFLoader(char *elfFileName):
  _elfFileName(elfFileName),
  _header(new ELFEHeader()),
  _zeroSection(new ELFSectionHeader()),
  _textSection(new ELFSectionHeader()),
  _relaSection(new ELFSectionHeader()),
  _dynSymSection(new ELFSectionHeader()),
  _shStrTabSection(new ELFSectionHeader()),
  _dynStrSection(new ELFSectionHeader()),
  _text(NULL),
  _symtab(NULL),
  _rela(NULL)
{
  initialize();
}

ELFLoader::~ELFLoader()
{
  fclose(_elfFile);
  delete _header;
  delete _zeroSection;
  delete _textSection;
  delete _relaSection;
  delete _dynSymSection;
  delete _shStrTabSection;
  delete _dynStrSection;
  free(_text);
  delete[] _symtab;
  delete[] _dynstr;
  delete[] _rela;
}

void ELFLoader::initialize()
{
  _elfFile = fopen(_elfFileName,"rb");
  if(_elfFile!=NULL)
  {
    size_t bytesRead = fread(_header,1,64,_elfFile);
    if(bytesRead!=64)
    {
      printf("Invalid ELF file format!");
      return;
    }
    if(fseek(_elfFile,_header->e_shoff,SEEK_SET))
    {
      printf("Invalid ELF file format!");
      return;
    }
    bytesRead = _header->e_shentsize;
    if(bytesRead!=fread(_zeroSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    if(bytesRead!=fread(_textSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    if(bytesRead!=fread(_relaSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    if(bytesRead!=fread(_dynSymSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    if(bytesRead!=fread(_shStrTabSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    if(bytesRead!=fread(_dynStrSection,1,_header->e_shentsize,_elfFile))
    {
      printf("Cannot read section!");
      return;
    }
    char shStrTab[_shStrTabSection->sh_size];
    fseek(_elfFile,_shStrTabSection->sh_offset,SEEK_SET);
    fread(shStrTab,1,_shStrTabSection->sh_size,_elfFile);
    memcpy(_zeroSectionName,&shStrTab[_zeroSection->sh_name],1);
    memcpy(_textSectionName,&shStrTab[_textSection->sh_name],6);
    memcpy(_relaSectionName,&shStrTab[_relaSection->sh_name],11);
    memcpy(_dynSymSectionName,&shStrTab[_dynSymSection->sh_name],8);
    memcpy(_shStrTabSectionName,&shStrTab[_shStrTabSection->sh_name],10);
    memcpy(_dynStrSectionName,&shStrTab[_dynStrSection->sh_name],8);
  }
}

void ELFLoader::loadTextSection()
{
  _text = malloc(_textSection->sh_size);
  fseek(_elfFile,_textSection->sh_offset,SEEK_SET);
  int bytesRead = _textSection->sh_size;
  if(bytesRead!=fread(_text,1,_textSection->sh_size,_elfFile))
  {
    printf("Cannot read .text section!");
    _text = NULL;
    return;
  }
}

void *ELFLoader::getTextSection()
{
  if(_text == NULL)
    loadTextSection();
  return _text;
}

void ELFLoader::loadSymTab()
{
  _symtab = new Elf64_Sym[_dynSymSection->sh_size/sizeof(Elf64_Sym)];
  fseek(_elfFile,_dynSymSection->sh_offset,SEEK_SET);
  int bytesRead = _dynSymSection->sh_size;
  if(bytesRead!=fread(_symtab,1,_dynSymSection->sh_size,_elfFile))
  {
    printf("Cannot read .symtab section!");
    _symtab=NULL;
    return;
  }
}

Elf64_Sym *ELFLoader::getSymbolTable()
{
  if(_symtab == NULL)
    loadSymTab();
  return _symtab;
}

void ELFLoader::loadDynStr()
{
  _dynstr = new char[_dynStrSection->sh_size];
  fseek(_elfFile,_dynStrSection->sh_offset,SEEK_SET);
  int bytesRead = _dynStrSection->sh_size;
  if(bytesRead!=fread(_dynstr,1,_dynStrSection->sh_size,_elfFile))
  {
    printf("Cannot read .dynstr section!");
    _dynstr = NULL;
    return;
  }
}

void ELFLoader::loadRela()
{
  _rela = new Elf64_Rela[_relaSection->sh_size/sizeof(Elf64_Rela)];
  fseek(_elfFile,_relaSection->sh_offset,SEEK_SET);
  int bytesRead = _relaSection->sh_size;
  if(bytesRead!=fread(_rela,1,_relaSection->sh_size,_elfFile))
  {
    printf("Cannot read .rela.text section!");
    _rela = NULL;
    return;
  }
}

#endif //LINUX
