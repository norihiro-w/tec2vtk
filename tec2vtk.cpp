/* Copyright 2009-2012 Norihiro Watanabe. All rights reserved.

tec2vtk is free software; you can redistribute and use in source and 
binary forms, with or without modification. tec2vtk is distributed in the 
hope that it will be useful but WITHOUT ANY WARRANTY; without even the 
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

*/


#define _CRT_SECURE_NO_WARNINGS //for MSVC

#define LASTUPDATE "April.16.2009"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

/************************************************************************/
/* Library to manipulate strings                                        */
/************************************************************************/
namespace StringLib
{
/// 
void getFileNameWithoutExtension(string const &str_file_name, string &str_file_name_base)
{
  string::size_type pos = str_file_name.find_last_of(".");
  if (pos == string::npos) {
    str_file_name_base = str_file_name;
  } else {
    str_file_name_base = str_file_name.substr(0, pos);
  }
};

///
char* trim(char* src, char ch = ' ') {
  char *ret = src;
  size_t size = strlen(src);
  //front
  for (size_t i=0; i<size; i++) {
    if (ret[0] == ch)
      ret++;
    else
      break;
  }
  size = strlen(ret);
  //back
  for (size_t i=size-1; i!=0; i--) {
    if (ret[i] == ch)
      ret[i] = '\0';
    else
      break;
  }

  return ret;
};

///
char* removeChar(char* src, char ch) {
  char *ret = src;
  char wrk_str[512];
  strncpy(wrk_str, src, 512);
  size_t size = strlen(src);
  int cur_pos = 0;
  memset(ret, '\0', 512);

  for (size_t i=0; i<size; i++) {
    if (wrk_str[i] != ch) {
      ret[cur_pos] = wrk_str[i];
      cur_pos++;
    }
  }

  return ret;
};

///
void toUpper(char *dest, char* src, int length, int begin, int count)
{
  memset(dest, '\0', length);
  strncpy(dest, src, length);
  for (int i=begin; i<begin+count; i++)
    dest[i] = toupper(src[i]);
}

void toUpperWithStopChar(char *dest, char* src, int length, int begin, char stop)
{
  memset(dest, '\0', length);
  strncpy(dest, src, length);
  for (int i=begin; i<length; i++) {
    if (src[i] == stop)
      break;
    dest[i] = toupper(src[i]);
  }
}

void removeCRLF(char* str_line)
{
  if (str_line[strlen(str_line)-1] == '\n')
    str_line[strlen(str_line)-1] = '\0';
  if (str_line[strlen(str_line)-1] == '\r')
    str_line[strlen(str_line)-1] = '\0';
}

void splitString(vector<string> &dest, const char *src, const char *delimiter)
{
  char tmp_buff[512];
  strncpy(tmp_buff, src, sizeof(tmp_buff));
  char *val = strtok(tmp_buff, ",");
  while (val!=NULL) {
    dest.push_back(val);
    val = strtok(NULL, ",");
  }
}

bool isVoidString(const char *str)
{
  const char ch_ignore[4] = {' ', '\n', '\r', '\t'};
  int size_ignore = sizeof (ch_ignore);
  size_t size = strlen(str);
  bool match = false;
  for (size_t i=0; i<size; i++) {
    match = false;
    for (int j=0; j<size_ignore; j++) {
      if (str[i] == ch_ignore[j]) {
        match = true;
        break;
      }
    }
    if (!match)
      return false;
  }
  return true;
}
};

/************************************************************************/
/* Tecplot sub data class: Finite element zone record                   */
/************************************************************************/
class CFemZoneRecord
{
public:
  map<string, string> zone_properties;
  //    string T,N,E,C,ZONETYPE,DT,DATAPACKING,NV,CONNECTIVITYSHAREZONE,STRANDID,SOLUTIONTIME,AUXDATA,PARENTZONE,VARLOCATION,VARSHARELIST,FACENEIGHBORMODE,FACENEIGHBORCONNECTLIST;

  string zone_title;
  long node_size;
  long element_size;
  string str_element_type;
  enum ElementType {UNDEFINED, POINT, LINE, TRIANGLE, QUAD, TETRA, HEXA, PRISM};
  ElementType element_type;
  vector<vector<string> > node_value_list;
  vector<vector<int> > element_nodes_list;
  string str_color;
public:
  CFemZoneRecord(void){};
  virtual ~CFemZoneRecord(void){};

  void Initialize() {
    zone_title.clear();
    node_size = 0;
    element_size = 0;
    str_element_type.clear();
    element_type = UNDEFINED;
    node_value_list.clear();
    element_nodes_list.clear();
    str_color.clear();
  };
};

/************************************************************************/
/* A Tecplot Data class                                                 */
/************************************************************************/
class CTecplotData
{
private:
  string tecplot_file_name;
  FILE* fp_tecplot_file;
  bool blnStoreOneRecord;

public:
  //FILE HEADER
  string title;
  vector<string> variables;
  //Record
  vector<CFemZoneRecord*> record_list;


public:
  CTecplotData(void);
  virtual ~CTecplotData(void);

public:
  bool OpenTecplotFile(const string &src_file);
  void CloseTecplotFile();
  void ReadFileHeader();
  CFemZoneRecord* ReadRecord();
  static bool WriteLegacyVTKFile(const char *str_file_name, CTecplotData* tecdata, CFemZoneRecord* wrk_zone);

private:
  void SplitRecordProperties(CFemZoneRecord *p_record, char *val);
};

CTecplotData::CTecplotData(void)
{
  blnStoreOneRecord = true;
}

CTecplotData::~CTecplotData(void)
{
  if (this->fp_tecplot_file != NULL)
    this->CloseTecplotFile();
  for (size_t i=0; i<record_list.size(); i++) {
    delete this->record_list[i];
  }
  this->record_list.clear();
}

bool CTecplotData::OpenTecplotFile(const string &src_file)
{
  this->tecplot_file_name = src_file;
  this->fp_tecplot_file = fopen(this->tecplot_file_name.c_str(), "r");
  if (this->fp_tecplot_file == NULL) {
    printf("FILE OPEN ERROR: %s\n", this->tecplot_file_name.c_str());
    return false;
  }
  return true;
}

void CTecplotData::CloseTecplotFile()
{
  if (this->fp_tecplot_file == NULL)
    return;

  fclose(this->fp_tecplot_file);
  this->fp_tecplot_file = NULL;
}

#define LINE_LENGTH 512

/// 
void CTecplotData::ReadFileHeader()
{
  if (this->fp_tecplot_file == NULL)
    return;

  char str_line[LINE_LENGTH], tmp_buff[LINE_LENGTH], *tmp_line;
  bool IsReadingVariables = false;
  long previous_fp_pos = 0;

  bool endFile = false;
  while ((endFile=!(fgets(str_line, sizeof(str_line), this->fp_tecplot_file) != 0)) == false) 
  {
    // Normalize line characters
    StringLib::removeCRLF(str_line);
    tmp_line = StringLib::trim(str_line);
    StringLib::toUpperWithStopChar(tmp_buff, tmp_line, sizeof(tmp_buff), 0, '=');

    // Read header information
    if (strstr(tmp_buff, "TITLE") != 0) {
      // Title 
      char *val = strtok(tmp_buff, "=");
      val = strtok(NULL, "=");
      this->title = StringLib::trim(StringLib::trim(val), '"');
    } else if ( strstr(tmp_buff, "VARIABLES") != 0) {
      // Variables 1
      char *val = strtok(tmp_buff, "=");
      val = strtok(NULL, "=");
      if (strstr(val, ",") != NULL) {
        val = strtok(val, ",");
        while (val!=NULL) {
          this->variables.push_back(string(StringLib::trim(StringLib::trim(val), '"')));
          val = strtok(NULL, ",");
        }
      } else {
        this->variables.push_back(string(StringLib::trim(StringLib::trim(val), '"')));
      }
      IsReadingVariables = true;
    } else if ( IsReadingVariables == true && strstr(tmp_buff, "ZONE") == 0) {
      // Variables 2
      char *val = NULL;
      if (strstr(tmp_buff, ",") != NULL) {
        val = strtok(tmp_buff, ",");
        while (val!=NULL) {
          this->variables.push_back(string(StringLib::trim(StringLib::trim(val), '"')));
          val = strtok(NULL, ",");
        }
      } else {
        this->variables.push_back(string(StringLib::trim(StringLib::trim(tmp_buff), '"')));
      }
    } else if ( strstr(tmp_buff, "ZONE") != 0) {
      // Finish reading header.
      fseek(this->fp_tecplot_file, previous_fp_pos, SEEK_SET);    
      return;
    }
    previous_fp_pos = ftell(this->fp_tecplot_file);
  }
}

///
void CTecplotData::SplitRecordProperties(CFemZoneRecord *p_record, char *val)
{
  char tmp_buff2[LINE_LENGTH];
  if (strstr(val, ",") != NULL) { // comma 
    vector<string> tmp_list;
    StringLib::splitString(tmp_list, val, ",");
    for (size_t i=0; i<tmp_list.size(); i++) {
      strncpy(tmp_buff2, tmp_list[i].c_str(), sizeof(tmp_buff2));
      char *val1 = strtok(tmp_buff2, "=");
      char *val2 = strtok(NULL, "=");
      p_record->zone_properties[StringLib::trim(val1)] = string(StringLib::trim(StringLib::trim(val2), '"'));
    }
  } else { // space
    char *val1 = strtok(val, "=");
    char *val2 = strtok(NULL, "=");
    p_record->zone_properties[StringLib::trim(val1)] = string(StringLib::trim(StringLib::trim(val2), '"'));
  }
}

///
CFemZoneRecord* CTecplotData::ReadRecord()
{
  if (this->fp_tecplot_file == NULL)
    return NULL;

  bool IsReadingZoneProperties = false;
  long previous_fp_pos = 0;
  CFemZoneRecord *p_record;
  if (this->blnStoreOneRecord && this->record_list.size()>0) {
    p_record = this->record_list.front();
    p_record->Initialize();
  } else {
    p_record = new CFemZoneRecord();
    p_record->Initialize();
    this->record_list.push_back(p_record);
  }

  //Record header
  char str_line[LINE_LENGTH], tmp_buff[LINE_LENGTH], *tmp_line;
  bool endFile = false;
  while ((endFile=!(fgets(str_line, sizeof(str_line), this->fp_tecplot_file) != 0)) == false) 
  {
    // Normalize line characters
    StringLib::removeCRLF(str_line);
    tmp_line = StringLib::trim(str_line);
    StringLib::toUpperWithStopChar(tmp_buff, tmp_line, sizeof(tmp_buff), 0, '=');

    if ( strstr(tmp_buff, "ZONE") == tmp_buff) {
      char *val = tmp_buff-1+sizeof("ZONE");
      this->SplitRecordProperties(p_record, val);
      IsReadingZoneProperties = true;
    } else if ( IsReadingZoneProperties && isalpha(tmp_buff[0]) ) {
      char *val = tmp_buff;
      this->SplitRecordProperties(p_record, val);
    } else if ( !isalpha(tmp_buff[0]) ) {
      fseek(this->fp_tecplot_file, previous_fp_pos, SEEK_SET);    
      break;
    }
    previous_fp_pos = ftell(this->fp_tecplot_file);
  }
  if (endFile == true) {
    return NULL;
  }

  //Analyze record header
  if (!p_record->zone_properties["T"].empty()) {
    sscanf(p_record->zone_properties["T"].c_str(), "%s", tmp_buff);
    p_record->zone_title = tmp_buff;
  } 
  if (!p_record->zone_properties["N"].empty()) {
    sscanf(p_record->zone_properties["N"].c_str(), "%ld", &p_record->node_size);
  }
  if (!p_record->zone_properties["E"].empty()) {
    sscanf(p_record->zone_properties["E"].c_str(), "%ld", &p_record->element_size);
  }
  if (!p_record->zone_properties["I"].empty()) {
    sscanf(p_record->zone_properties["I"].c_str(), "%ld", &p_record->node_size);
  }
  if (!p_record->zone_properties["ET"].empty()) {
    sscanf(p_record->zone_properties["ET"].c_str(), "%s", tmp_buff);
    p_record->str_element_type = tmp_buff;
    if (p_record->str_element_type.find("TETRAHEDRON") == string::npos
      && p_record->str_element_type.find("QUADRILATERAL") == string::npos
      && p_record->str_element_type.find("TRIANGLE") == string::npos
      && p_record->str_element_type.find("BRICK") == string::npos) {
        printf("ERROR: NOT SUPPORTED ELEMENT TYPE- %s\n", p_record->str_element_type.c_str());
        return false;
    }
  }
  if (!p_record->zone_properties["ZONETYPE"].empty()) {
    string &zone_type = p_record->zone_properties["ZONETYPE"];
    std::transform(zone_type.begin(), zone_type.end(), zone_type.begin(), ::toupper);
    if (zone_type.find("FETETRAHEDRON") != string::npos) {
      p_record->str_element_type = "TETRAHEDRON";
    } else if (zone_type.find("FEQUADRILATERAL") != string::npos) {
      p_record->str_element_type = "QUADRILATERAL";
    } else if (zone_type.find("FETRIANGLE") != string::npos) {
      p_record->str_element_type = "TRIANGLE";
    } else if (zone_type.find("FEBRICK") != string::npos) {
      p_record->str_element_type = "BRICK";
    } else {
      printf("ERROR: NOT SUPPORTED ELEMENT TYPE- %s\n", zone_type.c_str());
      return false;
    }
  }

  //Point data
  p_record->node_value_list.resize(p_record->node_size);
  const size_t variables_size = this->variables.size();
  if (p_record->zone_properties["DATAPACKING"] != "BLOCK") {
    for (long i=0; i<p_record->node_size; i++) 
    {
      for (size_t j=0; j<variables_size; j++) {
        fscanf(this->fp_tecplot_file, "%s", tmp_buff);
        p_record->node_value_list[i].push_back(string(StringLib::trim(tmp_buff)));
      }
    }
  } else {
    for (size_t i=0; i<variables_size; i++) 
    {
      for (long j=0; j<p_record->node_size; j++) {
        fscanf(this->fp_tecplot_file, "%s", tmp_buff);
        p_record->node_value_list[j].push_back(string(StringLib::trim(tmp_buff)));
      }
    }
  }

  fgets(str_line, sizeof(str_line), this->fp_tecplot_file);
  if(!StringLib::isVoidString(str_line)) {
    printf("ERROR: UNEXPECTED END OF POINT DATA: %s\n", str_line);
    return NULL;
  }


  //Element data
  p_record->element_nodes_list.resize(p_record->element_size);
  for (long i=0;i < p_record->element_size; i++) 
  {
    if (fgets(str_line, sizeof(str_line), this->fp_tecplot_file) == NULL) {
      printf("UNEXPECTED FILE END ERROR: Element data\n");
      return NULL;
    }
    StringLib::removeCRLF(str_line);
    tmp_line = StringLib::trim(str_line);

    char *val = NULL;
    val = strtok(tmp_line, " ");
    int tmp_id;
    while (val != NULL) {
      sscanf(val, "%d", &tmp_id);
      p_record->element_nodes_list[i].push_back(tmp_id);
      val = strtok(NULL, " ");
    }
  }

  if (p_record->element_type == CFemZoneRecord::UNDEFINED) {
    if (p_record->str_element_type.find("QUADRILATERAL") != string::npos) {
      if (p_record->element_nodes_list[0][0] == p_record->element_nodes_list[0][3]
      && p_record->element_nodes_list[0][1] == p_record->element_nodes_list[0][2]) {
        p_record->element_type = CFemZoneRecord::LINE;
      } else if (p_record->element_nodes_list[0][0] == p_record->element_nodes_list[0][3]
      && p_record->element_nodes_list[0][1] != p_record->element_nodes_list[0][2]) {
        p_record->element_type = CFemZoneRecord::TRIANGLE;
      } else {
        p_record->element_type = CFemZoneRecord::QUAD;
      }
    } else if (p_record->str_element_type.find("TETRAHEDRON") != string::npos) {
      p_record->element_type = CFemZoneRecord::TETRA;
    } else if (p_record->str_element_type.find("TRIANGLE") != string::npos) {
      p_record->element_type = CFemZoneRecord::TRIANGLE;
    } else if (p_record->str_element_type.find("BRICK") != string::npos) {
      if (p_record->element_nodes_list[0][0] == p_record->element_nodes_list[0][1]) {
        p_record->element_type = CFemZoneRecord::PRISM;
      } else {
        p_record->element_type = CFemZoneRecord::HEXA;
      }
    }
  }

  return p_record; 
}

///
bool CTecplotData::WriteLegacyVTKFile(const char *str_file_name, CTecplotData* tecdata, CFemZoneRecord* wrk_zone)
{
  FILE *fp = fopen(str_file_name, "w");

  fprintf(fp, "# vtk DataFile Version 2.0\n");
  fprintf(fp, "Unstructured Grid\n");
  fprintf(fp, "ASCII\n");
  fprintf(fp, "\n");
  fprintf(fp, "DATASET UNSTRUCTURED_GRID\n");
  //POINT
  fprintf(fp, "POINTS %d double\n", wrk_zone->node_size);
  int xyz[3] = {-1,-1,-1};
  for (int j=0; j<(int)tecdata->variables.size(); j++)
  {
    if (tecdata->variables[j] == "X" || tecdata->variables[j] == "x") xyz[0] = j;
    if (tecdata->variables[j] == "Y" || tecdata->variables[j] == "y") xyz[1] = j;
    if (tecdata->variables[j] == "Z" || tecdata->variables[j] == "z") xyz[2] = j;
  }
  for (size_t j=0; j<wrk_zone->node_value_list.size(); j++)
  {
    char *p_xyz[3];
    for (int k=0; k<3;k++) {
      if (xyz[k]==-1)
        p_xyz[k] = "0.0";
      else
        p_xyz[k] = (char*)wrk_zone->node_value_list[j][xyz[k]].c_str();
    }
    fprintf(fp, "%s %s %s\n", p_xyz[0], p_xyz[1], p_xyz[2]);
  }
  fprintf(fp, "\n");
  if (wrk_zone->element_size > 0) {
    //CELL
    int size_of_integer_list = 0;
    if (wrk_zone->element_type == CFemZoneRecord::TETRA) {
      size_of_integer_list = wrk_zone->element_size * (1 + 4); //ID + 4 node index
    } else if (wrk_zone->element_type == CFemZoneRecord::TRIANGLE) {
      size_of_integer_list = wrk_zone->element_size * (1 + 3);
    } else if (wrk_zone->element_type == CFemZoneRecord::HEXA) {
      size_of_integer_list = wrk_zone->element_size * (1 + 8);
    } else if (wrk_zone->element_type == CFemZoneRecord::LINE) {
      size_of_integer_list = wrk_zone->element_size * (1 + 2);
    } else if (wrk_zone->element_type == CFemZoneRecord::QUAD) {
      size_of_integer_list = wrk_zone->element_size * (1 + 4);
    } else if (wrk_zone->element_type == CFemZoneRecord::PRISM) {
      size_of_integer_list = wrk_zone->element_size * (1 + 6);
    } else {
      printf("not implemented yet... \n");
      fclose(fp);
      exit(0);
    }

    fprintf(fp, "CELLS %d %d\n", wrk_zone->element_size, size_of_integer_list);
    for (int j=0; j<(int)wrk_zone->element_nodes_list.size(); j++)
    {
      if (wrk_zone->element_type == CFemZoneRecord::TETRA) {
        fprintf(fp, "4 ");
        for (int k=0; k<4;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      } else if (wrk_zone->element_type == CFemZoneRecord::TRIANGLE) {
        fprintf(fp, "3 ");
        //TECPLOT NODE1, 2, 3, 1
        for (int k=0; k<3;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      } else if (wrk_zone->element_type == CFemZoneRecord::HEXA) {
        fprintf(fp, "8 ");
        for (int k=0; k<8;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      } else if (wrk_zone->element_type == CFemZoneRecord::LINE) {
        fprintf(fp, "2 ");
        //TECPLOT NODE1, 2, 2, 1
        for (int k=0; k<2;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      } else if (wrk_zone->element_type == CFemZoneRecord::QUAD) {
        fprintf(fp, "4 ");
        for (int k=0; k<4;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      } else if (wrk_zone->element_type == CFemZoneRecord::PRISM) {
        fprintf(fp, "6 ");
        //TECPLOT NODE1, 1, 2, 3, 4, 4, 5, 6
        for (int k=1; k<4;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
        for (int k=5; k<8;k++) {
          fprintf(fp, " %d", wrk_zone->element_nodes_list[j][k]-1);
        }
      }
      fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
    //CELLTYPE
    fprintf(fp, "CELL_TYPES %d\n", wrk_zone->element_size);
    int cell_type = 0;
    if (wrk_zone->element_type == CFemZoneRecord::TETRA) {
      cell_type = 10;
    } else if (wrk_zone->element_type == CFemZoneRecord::TRIANGLE) {
      cell_type = 5;
    } else if (wrk_zone->element_type == CFemZoneRecord::HEXA) {
      cell_type = 12;
    } else if (wrk_zone->element_type == CFemZoneRecord::LINE) {
      cell_type = 3;
    } else if (wrk_zone->element_type == CFemZoneRecord::QUAD) {
      cell_type = 9;
    } else if (wrk_zone->element_type == CFemZoneRecord::PRISM) {
      cell_type = 13;
    }
    for (int j=0; j<(int)wrk_zone->element_nodes_list.size(); j++)
    {
      fprintf(fp, "%d\n", cell_type);
    }
    fprintf(fp, "\n");
  } else {
    //CELL
    int size_of_integer_list = 0;
    size_of_integer_list = wrk_zone->node_size * (1 + 1);
    fprintf(fp, "CELLS %d %d\n", wrk_zone->node_size, size_of_integer_list);
    //
    for (int j=0; j<(int)wrk_zone->node_value_list.size(); j++)
    {
      fprintf(fp, "1 %d\n", j);
    }
    fprintf(fp, "\n");
    //CELLTYPE
    fprintf(fp, "CELL_TYPES %d\n", wrk_zone->node_size);
    fprintf(fp, "%d\n", 1);
    for (int j=0; j<wrk_zone->node_size; j++)
    {
      fprintf(fp, "%d\n", 1);
    }
    fprintf(fp, "\n");
  }

  //POINT DATA
  fprintf(fp, "POINT_DATA %d\n", wrk_zone->node_size);
  for (int j=0; j<(int)tecdata->variables.size(); j++)
  {
    if (tecdata->variables[j] == "X") continue;
    if (tecdata->variables[j] == "Y") continue;
    if (tecdata->variables[j] == "Z") continue;
    fprintf(fp, "SCALARS %s double 1\n", tecdata->variables[j].c_str());
    fprintf(fp, "LOOKUP_TABLE default\n");
    for (int k=0; k<(int)wrk_zone->node_value_list.size(); k++)
    {
      fprintf(fp, "%s\n", wrk_zone->node_value_list[k][j].c_str());
    }
  }

  //VECTOR DATA
  bool bVectorData = false;
  for (int j=0; j<(int)tecdata->variables.size(); j++)
  {
    if (tecdata->variables[j] == "VX"
      || tecdata->variables[j] == "VY"
      || tecdata->variables[j] == "VZ"
      || tecdata->variables[j] == "VELOCITY_X1"
      || tecdata->variables[j] == "VELOCITY_Y1"
      || tecdata->variables[j] == "VELOCITY_Z1") {
        bVectorData = true;
    }
  }
  if (bVectorData) {
    int xyz[3] = {-1,-1,-1};
    for (int j=0; j<(int)tecdata->variables.size(); j++)
    {
      if (tecdata->variables[j] == "VX" || tecdata->variables[j] == "VELOCITY_X1") xyz[0] = j;
      if (tecdata->variables[j] == "VY" || tecdata->variables[j] == "VELOCITY_Y1") xyz[1] = j;
      if (tecdata->variables[j] == "VZ" || tecdata->variables[j] == "VELOCITY_Z1") xyz[2] = j;
    }
    fprintf(fp, "VECTORS velocity double\n");
    for (int j=0; j<(int)wrk_zone->node_value_list.size(); j++)
    {
      for (int k=0; k<3; k++) {
        if (xyz[k]!=-1) {
          fprintf(fp, "%s ", wrk_zone->node_value_list[j][xyz[k]].c_str());
        } else {
          fprintf(fp, "0.0");
        }
      }
      fprintf(fp, "\n");
    }
  }

  fclose(fp);

  return true;
}

int main(int argc, char** argv)
{
  char str_src_file[256], str_out_file[256], tmp_buff[128];
  int output_zone_id_begin = 0; //default
  if (argc < 3) {
    printf("THIS IS A DATA CONVERSION TOOL FROM TECPLOT TO VTK. \n");
    printf("* LAST UPDATE: %s\n", LASTUPDATE);
    printf("* YOU CAN ALSO START WITH ARGUMENTS: tec2vtk.exe <TECPLOT FILE PATH> <VTK FILE PATH> <OPTION: STARTING ZONE No.>\n");
    printf("--\n");
    printf("\n");
    if (argc == 1) {
      printf("ENTER YOUR TECPLOT FILE NAME.\n");
      printf("> ");
      scanf("%s", str_src_file);
      printf("\n");
    } else {
      strncpy(str_src_file, argv[1], sizeof(str_src_file));
    }
    printf("[INITIAL SETTING]\n");
    printf("VTK FILE NAME: (BASE NAME OF TECPLOT FILE).VTK\n");
    printf("ZONE RANGE:    ALL\n");
    printf("\n");
    printf("USE INITIAL SETTING? (Y/N)\n");
    printf("> ");
    scanf("%s", tmp_buff);
    if (toupper(tmp_buff[0]) != 'Y') {
      printf("\n\nENTER BASE NAME OF VTK FILE CREATED.\n");
      printf("> ");
      scanf("%s", str_out_file);
      printf("\n\nENTER ZONE(TIMESTEP) INDEX WHERE YOU LIKE TO START CONVERSION. IF YOU SET 0, ALL DATA WILL BE CONVERTED.\n");
      printf("> ");
      scanf("%d", &output_zone_id_begin);
    } else {
      strncpy(str_out_file, str_src_file, sizeof(str_out_file)); //use tecplot file name as vtk
    }
  } else {
    strncpy(str_src_file, argv[1], sizeof(str_src_file));
    strncpy(str_out_file, argv[2], sizeof(str_out_file));
    if (argc == 4)
      sscanf(argv[3], "%d", &output_zone_id_begin);
  }

  //----------------------------------------------------------------------
  CTecplotData tecdata;
  if (!tecdata.OpenTecplotFile(str_src_file)) {
    return 0;
  }

  printf("\n\n-- PROCESS LOG --\n");

  CFemZoneRecord *zone;
  string str_file_name_base;
  string str_out_file_name;
  StringLib::getFileNameWithoutExtension(string(str_out_file), str_file_name_base);

  printf("[SETTING]\n");
  printf("VTK FILE NAME: %s\n", str_file_name_base.c_str());
  printf("ZONE RANGE:    FROM %d\n", output_zone_id_begin);
  printf("\n");
  printf("START CONVERSION.\n");

  //read TECPLOT file
  int zone_count = 0;
  printf("READING ZONE: ");
  tecdata.ReadFileHeader();
  while ((zone = tecdata.ReadRecord())!=NULL) {
    printf("%d ", zone_count);
    if (zone_count+1 > output_zone_id_begin ) {
      ostringstream s1;
      s1 << str_file_name_base << zone_count << ".vtk";
      if (!CTecplotData::WriteLegacyVTKFile(s1.str().c_str(), &tecdata, zone)) {
        tecdata.CloseTecplotFile();
        printf("\nERROR: writeVTKFile() \n");
        return -1;
      }
    }
    zone_count++;
  }
  tecdata.CloseTecplotFile();

  printf("\nFINISH.\n");

  return 0;
};

