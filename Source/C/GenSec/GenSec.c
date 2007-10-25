/*++

Copyright (c) 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  GenSection.c

Abstract:

  Creates output file that is a properly formed section per the FV spec.

--*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Common/UefiBaseTypes.h>
#include <Common/PiFirmwareFile.h>
#include <Protocol/GuidedSectionExtraction.h>

#include "CommonLib.h"
#include "Compress.h"
#include "Crc32.h"
#include "EfiUtilityMsgs.h"

//
// GenSec Tool Information
//
#define UTILITY_NAME            "GenSec"
#define UTILITY_MAJOR_VERSION   0
#define UTILITY_MINOR_VERSION   1

#define MAX_SECTION_SIZE        0x1000000

STATIC CHAR8      *mSectionTypeName[] = {
  NULL,                                 // 0x00 - reserved
  "EFI_SECTION_COMPRESSION",            // 0x01
  "EFI_SECTION_GUID_DEFINED",           // 0x02
  NULL,                                 // 0x03 - reserved
  NULL,                                 // 0x04 - reserved
  NULL,                                 // 0x05 - reserved
  NULL,                                 // 0x06 - reserved
  NULL,                                 // 0x07 - reserved
  NULL,                                 // 0x08 - reserved
  NULL,                                 // 0x09 - reserved
  NULL,                                 // 0x0A - reserved
  NULL,                                 // 0x0B - reserved
  NULL,                                 // 0x0C - reserved
  NULL,                                 // 0x0D - reserved
  NULL,                                 // 0x0E - reserved
  NULL,                                 // 0x0F - reserved
  "EFI_SECTION_PE32",                   // 0x10
  "EFI_SECTION_PIC",                    // 0x11
  "EFI_SECTION_TE",                     // 0x12
  "EFI_SECTION_DXE_DEPEX",              // 0x13
  "EFI_SECTION_VERSION",                // 0x14
  "EFI_SECTION_USER_INTERFACE",         // 0x15
  "EFI_SECTION_COMPATIBILITY16",        // 0x16
  "EFI_SECTION_FIRMWARE_VOLUME_IMAGE",  // 0x17
  "EFI_SECTION_FREEFORM_SUBTYPE_GUID",  // 0x18
  "EFI_SECTION_RAW",                    // 0x19
  NULL,                                 // 0x1A
  "EFI_SECTION_PEI_DEPEX"               // 0x1B
};

STATIC CHAR8      *mCompressionTypeName[]    = { "PI_NONE", "PI_STD" };
STATIC CHAR8      *mGUIDedSectionAttribue[]  = { NULL, "PROCESSING_REQUIRED", "AUTH_STATUS_VALID"};

//
// Crc32 GUID section related definitions.
//
typedef struct {
  EFI_GUID_DEFINED_SECTION  GuidSectionHeader;
  UINT32                    CRC32Checksum;
} CRC32_SECTION_HEADER;

STATIC EFI_GUID  mZeroGuid                 = {0x0, 0x0, 0x0, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}};
STATIC EFI_GUID  mEfiCrc32SectionGuid      = EFI_CRC32_GUIDED_SECTION_EXTRACTION_PROTOCOL_GUID;

STATIC
VOID 
Version (
  VOID
  )
/*++

Routine Description:

  Print out version information for this utility.

Arguments:

  None
  
Returns:

  None
  
--*/ 
{
  fprintf (stdout, "%s Version %d.%d\n", UTILITY_NAME, UTILITY_MAJOR_VERSION, UTILITY_MINOR_VERSION);
}

STATIC
VOID
Usage (
  VOID
  )
/*++

Routine Description:

  Print Help message.

Arguments:

  VOID

Returns:

  None

--*/
{
  //
  // Summary usage
  //
  fprintf (stdout, "\nUsage: %s [options] [input_file]\n\n", UTILITY_NAME);
  
  //
  // Copyright declaration
  // 
  fprintf (stdout, "Copyright (c) 2007, Intel Corporation. All rights reserved.\n\n");

  //
  // Details Option
  //
  fprintf (stdout, "Options:\n");
  fprintf (stdout, "  -o FileName, --outputfile FileName\n\
                        File is the SectionFile to be created.\n");
  fprintf (stdout, "  -s [SectionType], --sectiontype [SectionType]\n\
                        SectionType defined in PI spec is one type of\n\
                        EFI_SECTION_COMPRESSION, EFI_SECTION_GUID_DEFINED,\n\
                        EFI_SECTION_PE32, EFI_SECTION_PIC, EFI_SECTION_TE,\n\
                        EFI_SECTION_DXE_DEPEX, EFI_SECTION_COMPATIBILITY16,\n\
                        EFI_SECTION_USER_INTERFACE, EFI_SECTION_VERSION,\n\
                        EFI_SECTION_FIRMWARE_VOLUME_IMAGE, EFI_SECTION_RAW,\n\
                        EFI_SECTION_FREEFORM_SUBTYPE_GUID,\n\
                        EFI_SECTION_PEI_DEPEX. if sectiontype is not given, \n\
                        EFI_SECTION_ALL is default type.\n");
  fprintf (stdout, "  -c [Type], --compress [Type]\n\
                        Compress method type can be PI_NONE or PI_STD.\n\
                        if Type is not given, PI_STD is default type.\n"); 
  fprintf (stdout, "  -g GuidValue, --vendor GuidValue\n\
                        GuidValue is one specific vendor guid value.\n\
                        Its format is 00000000-0000-0000-0000-000000000000\n");
  fprintf (stdout, "  -r GuidAttr, --attributes GuidAttr\n\
                        GuidAttr is guid section atttributes, which may be\n\
                        PROCESSING_REQUIRED or AUTH_STATUS_VALID\n");
  fprintf (stdout, "  -n String, --name String\n\
                        String is a NULL terminated string used in Ui section.\n");
  fprintf (stdout, "  -j Number, --buildnumber Number\n\
                        Number is an integer value between 0000 and 9999\n\
                        used in Ver section.\n");
  fprintf (stdout, "  -v, --verbose         Turn on verbose output with informational messages.\n");
  fprintf (stdout, "  -q, --quiet           Disable all messages except key message and fatal error\n");
  fprintf (stdout, "  -d, --debug level     Enable debug messages, at input debug level.\n");
  fprintf (stdout, "  --version             Show program's version number and exit.\n");
  fprintf (stdout, "  -h, --help            Show this help message and exit.\n");
}

VOID
Ascii2UnicodeWriteString (
  CHAR8    *String,
  FILE     *OutFile
  )
/*++

Routine Description:

  Write ascii string as unicode string format to FILE 

Arguments:

  String    - Pointer to string that is written to FILE.
  OutFile   - Pointer to FILE

Returns:

  NULL

--*/
{
  UINT32 Index;
  UINT8  AsciiNull;

  AsciiNull = 0;

  //
  // Next, write out the string... Convert ASCII to Unicode in the process.
  //
  Index = 0;
  do {
    fwrite (&String[Index], 1, 1, OutFile);
    fwrite (&AsciiNull, 1, 1, OutFile);
  } while (String[Index++] != 0);
} 

STATUS
GenSectionCommonLeafSection (
  CHAR8   **InputFileName,
  UINT32  InputFileNum,
  UINT8   SectionType,
  FILE    *OutFile
  )
/*++
        
Routine Description:
           
  Generate a leaf section of type other than EFI_SECTION_VERSION
  and EFI_SECTION_USER_INTERFACE. Input file must be well formed.
  The function won't validate the input file's contents. For
  common leaf sections, the input file may be a binary file.
  The utility will add section header to the file.
            
Arguments:
               
  InputFileName  - Name of the input file.
                
  InputFileNum   - Number of input files. Should be 1 for leaf section.

  SectionType    - A valid section type string

  OutFile        - Output file handle

Returns:
                       
  STATUS_ERROR            - can't continue
  STATUS_SUCCESS          - successful return

--*/
{
  UINT32                    InputFileLength;
  FILE                      *InFile;
  UINT8                     *Buffer;
  UINT32                    TotalLength;
  EFI_COMMON_SECTION_HEADER CommonSect;
  STATUS                    Status;

  if (InputFileNum > 1) {
    Error (NULL, 0, 2000, "Invalid paramter", "more than one input file specified");
    return STATUS_ERROR;
  } else if (InputFileNum < 1) {
    Error (NULL, 0, 2000, "Invalid paramter", "no input file specified");
    return STATUS_ERROR;
  }
  //
  // Open the input file
  //
  InFile = fopen (InputFileName[0], "rb");
  if (InFile == NULL) {
    Error (NULL, 0, 0001, "Error opening file", InputFileName[0]);
    return STATUS_ERROR;
  }

  Status  = STATUS_ERROR;
  Buffer  = NULL;
  //
  // Seek to the end of the input file so we can determine its size
  //
  fseek (InFile, 0, SEEK_END);
  InputFileLength = ftell (InFile);
  fseek (InFile, 0, SEEK_SET);
  DebugMsg (NULL, 0, 9, "Input file", "File name is %s and File size is %d bytes", InputFileName[0], InputFileLength);
  //
  // Fill in the fields in the local section header structure
  //
  CommonSect.Type = (EFI_SECTION_TYPE) SectionType;
  TotalLength     = sizeof (CommonSect) + InputFileLength;
  //
  // Size must fit in 3 bytes
  //
  if (TotalLength >= MAX_SECTION_SIZE) {
    Error (NULL, 0, 2000, "Invalid paramter", "%s file size (0x%X) exceeds section size limit(%dM).", InputFileName[0], TotalLength, MAX_SECTION_SIZE>>20);
    goto Done;
  }
  VerboseMsg ("the size of the created section file is %d bytes", TotalLength);
  //
  // Now copy the size into the section header and write out the section header
  //
  memcpy (&CommonSect.Size, &TotalLength, 3);
  fwrite (&CommonSect, sizeof (CommonSect), 1, OutFile);
  //
  // Allocate a buffer to read in the contents of the input file. Then
  // read it in as one block and write it to the output file.
  //
  if (InputFileLength != 0) {
    Buffer = (UINT8 *) malloc ((size_t) InputFileLength);
    if (Buffer == NULL) {
      Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated"); 
      goto Done;
    }

    if (fread (Buffer, (size_t) InputFileLength, 1, InFile) != 1) {
      Error (NULL, 0, 0004, "Error reading file", InputFileName[0]);
      goto Done;
    }

    if (fwrite (Buffer, (size_t) InputFileLength, 1, OutFile) != 1) {
      Error (NULL, 0, 0002, "Error writing file", NULL);
      goto Done;
    }
  }

  Status = STATUS_SUCCESS;

Done:
  fclose (InFile);
  if (Buffer != NULL) {
    free (Buffer);
  }

  return Status;
}

EFI_STATUS
GetSectionContents (
  CHAR8   **InputFileName,
  UINT32  InputFileNum,
  UINT8   *FileBuffer,
  UINT32  *BufferLength
  )
/*++
        
Routine Description:
           
  Get the contents of all section files specified in InputFileName
  into FileBuffer.
            
Arguments:
               
  InputFileName  - Name of the input file.
                
  InputFileNum   - Number of input files. Should be at least 1.

  FileBuffer     - Output buffer to contain data

  BufferLength   - On input, this is size of the FileBuffer. 
                   On output, this is the actual length of the data.

Returns:
                       
  EFI_SUCCESS on successful return
  EFI_INVALID_PARAMETER if InputFileNum is less than 1 or BufferLength point is NULL.
  EFI_ABORTED if unable to open input file.
  EFI_BUFFER_TOO_SMALL FileBuffer is not enough to contain all file data.
--*/
{
  UINT32   Size;
  UINT32   FileSize;
  UINT32   Index;
  FILE    *InFile;

  if (InputFileNum < 1) {
    Error (NULL, 0, 2000, "Invalid paramter", "must specify at least one input file");
    return EFI_INVALID_PARAMETER;
  }

  if (BufferLength == NULL) {
    Error (NULL, 0, 2000, "Invalid paramter", "BufferLength can't be NULL");
    return EFI_INVALID_PARAMETER;
  }

  Size = 0;
  //
  // Go through our array of file names and copy their contents
  // to the output buffer.
  //
  for (Index = 0; Index < InputFileNum; Index++) {
    //
    // make sure section ends on a DWORD boundary
    //
    while ((Size & 0x03) != 0) {
      if (FileBuffer != NULL && Size < *BufferLength) {
        FileBuffer[Size] = 0;
      }
      Size++;
    }
    
    // 
    // Open file and read contents
    //
    InFile = fopen (InputFileName[Index], "rb");
    if (InFile == NULL) {
      Error (NULL, 0, 0001, "Error opening file", InputFileName[Index]);
      return EFI_ABORTED;
    }

    fseek (InFile, 0, SEEK_END);
    FileSize = ftell (InFile);
    fseek (InFile, 0, SEEK_SET);
    DebugMsg (NULL, 0, 9, "Input files", "the input file name is %s and the size is %d bytes", InputFileName[Index], FileSize); 
    //
    // Now read the contents of the file into the buffer
    // Buffer must be enough to contain the file content.
    //
    if (FileSize > 0 && FileBuffer != NULL && (Size + FileSize) <= *BufferLength) {
      if (fread (FileBuffer + Size, (size_t) FileSize, 1, InFile) != 1) {
        Error (NULL, 0, 0004, "Error reading file", InputFileName[Index]);
        fclose (InFile);
        return EFI_ABORTED;
      }
    }

    fclose (InFile);
    Size += FileSize;
  }
  
  //
  // Set the real required buffer size.
  //
  if (Size > *BufferLength) {
    *BufferLength = Size;
    return EFI_BUFFER_TOO_SMALL;
  } else {
    *BufferLength = Size;
    return EFI_SUCCESS;
  }
}

EFI_STATUS
GenSectionCompressionSection (
  CHAR8    **InputFileName,
  UINT32  InputFileNum,
  UINT8   SectCompSubType,
  FILE    *OutFile
  )
/*++
        
Routine Description:
           
  Generate an encapsulating section of type EFI_SECTION_COMPRESSION
  Input file must be already sectioned. The function won't validate
  the input files' contents. Caller should hand in files already 
  with section header.
            
Arguments:
               
  InputFileName  - Name of the input file.
                
  InputFileNum   - Number of input files. Should be at least 1.

  SectCompSubType - Specify the compression algorithm requested. 
  
  OutFile        - Output file handle

Returns:
                       
  EFI_SUCCESS           on successful return
  EFI_INVALID_PARAMETER if InputFileNum is less than 1
  EFI_ABORTED           if unable to open input file.
  EFI_OUT_OF_RESOURCES  No resource to complete the operation.
--*/
{
  UINT32                  TotalLength;
  UINT32                  InputLength;
  UINT32                  CompressedLength;
  UINT8                   *FileBuffer;
  UINT8                   *OutputBuffer;
  EFI_STATUS              Status;
  EFI_COMPRESSION_SECTION CompressionSect;
  COMPRESS_FUNCTION       CompressFunction;

  InputLength       = 0;
  FileBuffer        = NULL;
  OutputBuffer      = NULL;
  CompressedLength  = 0;
  //
  // read all input file contents into a buffer
  // first get the size of all file contents
  //
  Status = GetSectionContents (
            InputFileName,
            InputFileNum,
            FileBuffer,
            &InputLength
            );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    FileBuffer = (UINT8 *) malloc (InputLength);
    if (FileBuffer == NULL) {
      Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated");
      return EFI_OUT_OF_RESOURCES;
    }
    //
    // read all input file contents into a buffer
    //
    Status = GetSectionContents (
              InputFileName,
              InputFileNum,
              FileBuffer,
              &InputLength
              );
  }

  if (EFI_ERROR (Status)) {
    if (FileBuffer != NULL) {
      free (FileBuffer);
    }
    return Status;
  }

  CompressFunction = NULL;

  //
  // Now data is in FileBuffer, compress the data
  //
  switch (SectCompSubType) {
  case EFI_NOT_COMPRESSED:
    CompressedLength = InputLength;
    break;

  case EFI_STANDARD_COMPRESSION:
    CompressFunction = (COMPRESS_FUNCTION) EfiCompress;
    break;

  default:
    Error (NULL, 0, 2000, "Invalid paramter", "unknown compression type");
    free (FileBuffer);
    return EFI_ABORTED;
  }

  if (CompressFunction != NULL) {

    Status = CompressFunction (FileBuffer, InputLength, OutputBuffer, &CompressedLength);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      OutputBuffer = malloc (CompressedLength);
      if (!OutputBuffer) {
        free (FileBuffer);
        return EFI_OUT_OF_RESOURCES;
      }

      Status = CompressFunction (FileBuffer, InputLength, OutputBuffer, &CompressedLength);
    }

    free (FileBuffer);
    FileBuffer = OutputBuffer;

    if (EFI_ERROR (Status)) {
      if (FileBuffer != NULL) {
        free (FileBuffer);
      }

      return Status;
    }
  }

  DebugMsg (NULL, 0, 9, "comprss file size", 
            "the original section size is %d bytes and the compressed section size is %d bytes", InputLength, CompressedLength);
  TotalLength = CompressedLength + sizeof (EFI_COMPRESSION_SECTION);
  if (TotalLength >= MAX_SECTION_SIZE) {
    Error (NULL, 0, 2000, "Invalid paramter", "The size of all files exceeds section size limit(%dM).", MAX_SECTION_SIZE>>20);
    if (FileBuffer != NULL) {
      free (FileBuffer);
    }
    if (OutputBuffer != NULL) {
      free (OutputBuffer);
    }
    return STATUS_ERROR;
  }
  VerboseMsg ("the size of the created section file is %d bytes", TotalLength);

  //
  // Add the section header for the compressed data
  //
  CompressionSect.CommonHeader.Type     = EFI_SECTION_COMPRESSION;
  CompressionSect.CommonHeader.Size[0]  = (UINT8) (TotalLength & 0xff);
  CompressionSect.CommonHeader.Size[1]  = (UINT8) ((TotalLength & 0xff00) >> 8);
  CompressionSect.CommonHeader.Size[2]  = (UINT8) ((TotalLength & 0xff0000) >> 16);
  CompressionSect.CompressionType       = SectCompSubType;
  CompressionSect.UncompressedLength    = InputLength;

  fwrite (&CompressionSect, sizeof (CompressionSect), 1, OutFile);
  fwrite (FileBuffer, CompressedLength, 1, OutFile);
  free (FileBuffer);
  return EFI_SUCCESS;
}

EFI_STATUS
GenSectionGuidDefinedSection (
  CHAR8    **InputFileName,
  UINT32   InputFileNum,
  EFI_GUID *VendorGuid,
  UINT16   DataAttribute,
  FILE     *OutFile
  )
/*++
        
Routine Description:
           
  Generate an encapsulating section of type EFI_SECTION_GUID_DEFINED
  Input file must be already sectioned. The function won't validate
  the input files' contents. Caller should hand in files already 
  with section header.
            
Arguments:
               
  InputFileName - Name of the input file.
                
  InputFileNum  - Number of input files. Should be at least 1.

  VendorGuid    - Specify vendor guid value.

  DataAttribute - Specify attribute for the vendor guid data. 
  
  OutFile       - Output file handle

Returns:
                       
  EFI_SUCCESS on successful return
  EFI_INVALID_PARAMETER if InputFileNum is less than 1
  EFI_ABORTED if unable to open input file.
  EFI_OUT_OF_RESOURCES  No resource to complete the operation.

--*/
{
  UINT32                TotalLength;
  UINT32                InputLength;
  UINT8                 *FileBuffer;
  UINT32                Crc32Checksum;
  EFI_STATUS            Status;
  CRC32_SECTION_HEADER  Crc32GuidSect;
  EFI_GUID_DEFINED_SECTION  VendorGuidSect;

  InputLength = 0;
  FileBuffer  = NULL;
  //
  // read all input file contents into a buffer
  // first get the size of all file contents
  //
  Status = GetSectionContents (
            InputFileName,
            InputFileNum,
            FileBuffer,
            &InputLength
            );

  if (Status == EFI_BUFFER_TOO_SMALL) {
    FileBuffer = (UINT8 *) malloc (InputLength);
    if (FileBuffer == NULL) {
      Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated");
      return EFI_OUT_OF_RESOURCES;
    }
    //
    // read all input file contents into a buffer
    //
    Status = GetSectionContents (
              InputFileName,
              InputFileNum,
              FileBuffer,
              &InputLength
              );
  }

  if (EFI_ERROR (Status)) {
    if (FileBuffer != NULL) {
      free (FileBuffer);
    }
    return Status;
  }

  //
  // Now data is in FileBuffer
  //
  if (CompareGuid (VendorGuid, &mEfiCrc32SectionGuid) == 0) {
    //
    // Default Guid section is CRC32.
    //
    Crc32Checksum = 0;
    CalculateCrc32 (FileBuffer, InputLength, &Crc32Checksum);

    TotalLength = InputLength + sizeof (CRC32_SECTION_HEADER);
    if (TotalLength >= MAX_SECTION_SIZE) {
      Error (NULL, 0, 2000, "Invalid paramter", "The size of all files exceeds section size limit(%dM).", MAX_SECTION_SIZE>>20);
      free (FileBuffer);
      return STATUS_ERROR;
    }

    Crc32GuidSect.GuidSectionHeader.CommonHeader.Type     = EFI_SECTION_GUID_DEFINED;
    Crc32GuidSect.GuidSectionHeader.CommonHeader.Size[0]  = (UINT8) (TotalLength & 0xff);
    Crc32GuidSect.GuidSectionHeader.CommonHeader.Size[1]  = (UINT8) ((TotalLength & 0xff00) >> 8);
    Crc32GuidSect.GuidSectionHeader.CommonHeader.Size[2]  = (UINT8) ((TotalLength & 0xff0000) >> 16);
    memcpy (&(Crc32GuidSect.GuidSectionHeader.SectionDefinitionGuid), &mEfiCrc32SectionGuid, sizeof (EFI_GUID));
    Crc32GuidSect.GuidSectionHeader.Attributes  = EFI_GUIDED_SECTION_AUTH_STATUS_VALID;
    Crc32GuidSect.GuidSectionHeader.DataOffset  = sizeof (CRC32_SECTION_HEADER);
    Crc32GuidSect.CRC32Checksum                 = Crc32Checksum;
    fwrite (&Crc32GuidSect, sizeof (Crc32GuidSect), 1, OutFile);  

  } else {
    TotalLength = InputLength + sizeof (EFI_GUID_DEFINED_SECTION);
    if (TotalLength >= MAX_SECTION_SIZE) {
      Error (NULL, 0, 2000, "Invalid paramter", "The size of all files exceeds section size limit(%dM).", MAX_SECTION_SIZE>>20);
      free (FileBuffer);
      return STATUS_ERROR;
    }

    VendorGuidSect.CommonHeader.Type     = EFI_SECTION_GUID_DEFINED;
    VendorGuidSect.CommonHeader.Size[0]  = (UINT8) (TotalLength & 0xff);
    VendorGuidSect.CommonHeader.Size[1]  = (UINT8) ((TotalLength & 0xff00) >> 8);
    VendorGuidSect.CommonHeader.Size[2]  = (UINT8) ((TotalLength & 0xff0000) >> 16);
    memcpy (&(VendorGuidSect.SectionDefinitionGuid), VendorGuid, sizeof (EFI_GUID));
    VendorGuidSect.Attributes  = DataAttribute;
    VendorGuidSect.DataOffset  = sizeof (EFI_GUID_DEFINED_SECTION);
    fwrite (&VendorGuidSect, sizeof (EFI_GUID_DEFINED_SECTION), 1, OutFile);  
    DebugMsg (NULL, 0, 9, "Guided section", "Data offset is %d", VendorGuidSect.DataOffset);
  }
  VerboseMsg ("the size of the created section file is %d bytes", TotalLength);

  fwrite (FileBuffer, InputLength, 1, OutFile);
  free (FileBuffer);
  
  return EFI_SUCCESS;
}

int
main (
  int  argc,
  char *argv[]
  )
/*++

Routine Description:

  Main

Arguments:

  command line parameters

Returns:

  EFI_SUCCESS    Section header successfully generated and section concatenated.
  EFI_ABORTED    Could not generate the section
  EFI_OUT_OF_RESOURCES  No resource to complete the operation.

--*/
{
  UINT32                    Index;
  UINT32                    InputFileNum;
  FILE                      *InFile;
  FILE                      *OutFile;
  CHAR8                     **InputFileName;
  CHAR8                     *OutputFileName;
  CHAR8                     *SectionName;
  CHAR8                     *CompressionName;
  CHAR8                     *StringBuffer;
  EFI_GUID                  VendorGuid = mZeroGuid;
  INT32                     VersionNumber;
  UINT8                     SectType;
  UINT8                     SectCompSubType;
  UINT16                    SectGuidAttribute; 
  EFI_COMMON_SECTION_HEADER CommonSect;
  UINT32                    InputLength;
  UINT8                     *FileBuffer;
  EFI_STATUS                Status;
  UINT64                    LogLevel;
  
  InputFileName         = NULL;
  OutputFileName        = NULL;
  SectionName           = NULL;
  CompressionName       = NULL;
  StringBuffer          = "";
  InFile                = NULL;
  OutFile               = NULL;
  VersionNumber         = 0;
  InputFileNum          = 0;
  SectType              = EFI_SECTION_ALL;
  SectCompSubType       = 0;
  SectGuidAttribute     = 0;
  FileBuffer            = NULL;
  InputLength           = 0;
  Status                = STATUS_SUCCESS;
  LogLevel              = 0;
  
  SetUtilityName (UTILITY_NAME);
  
  if (argc == 1) {
    Error (NULL, 0, 1001, "Missing options", "No options input");
    Usage ();
    return STATUS_ERROR;
  }

  //
  // Parse command line
  //
  argc --;
  argv ++;

  if ((stricmp (argv[0], "-h") == 0) || (stricmp (argv[0], "--help") == 0)) {
    Version ();
    Usage ();
    return STATUS_SUCCESS;    
  }

  if (stricmp (argv[0], "--version") == 0) {
    Version ();
    return STATUS_SUCCESS;    
  }

  while (argc > 0) {
    if ((stricmp (argv[0], "-s") == 0) || (stricmp (argv[0], "--SectionType") == 0)) {
      SectionName = argv[1];
      argc -= 2;
      argv += 2;
      continue; 
    }

    if ((stricmp (argv[0], "-o") == 0) || (stricmp (argv[0], "--outputfile") == 0)) {
      OutputFileName = argv[1];
      argc -= 2;
      argv += 2;
      continue; 
    }

    if ((stricmp (argv[0], "-c") == 0) || (stricmp (argv[0], "--compress") == 0)) {
      CompressionName = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }

    if ((stricmp (argv[0], "-g") == 0) || (stricmp (argv[0], "--vendor") == 0)) {
      Status = StringToGuid (argv[1], &VendorGuid);
      if (EFI_ERROR (Status)) {
        Error (NULL, 0, 1003, "Invalid option value", "%s = %s", argv[0], argv[1]);
        goto Finish;
      }
      argc -= 2;
      argv += 2;
      continue;
    }

    if ((stricmp (argv[0], "-r") == 0) || (stricmp (argv[0], "--attributes") == 0)) {
      if (stricmp (argv[1], mGUIDedSectionAttribue[EFI_GUIDED_SECTION_PROCESSING_REQUIRED]) == 0) {
        SectGuidAttribute |= EFI_GUIDED_SECTION_PROCESSING_REQUIRED;
      } else if (stricmp (argv[1], mGUIDedSectionAttribue[EFI_GUIDED_SECTION_AUTH_STATUS_VALID]) == 0) {
        SectGuidAttribute |= EFI_GUIDED_SECTION_AUTH_STATUS_VALID;
      } else {
        Error (NULL, 0, 1003, "Invalid option value", "%s = %s", argv[0], argv[1]);
        goto Finish;
      }
      argc -= 2;
      argv += 2;
      continue;
    }

    if ((stricmp (argv[0], "-n") == 0) || (stricmp (argv[0], "--name") == 0)) {
      StringBuffer = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }

    if ((stricmp (argv[0], "-j") == 0) || (stricmp (argv[0], "--buildnumber") == 0)) {
      //
      // Verify string is a integrator number
      //
      for (Index = 0; Index < strlen (argv[1]); Index++) {
        if ((argv[1][Index] != '-') && (isdigit (argv[1][Index]) == 0)) {
          Error (NULL, 0, 1003, "Invalid option value", "%s = %s", argv[0], argv[1]);
          goto Finish;
        }
      }

      sscanf (argv[1], "%d", &VersionNumber);
      argc -= 2;
      argv += 2;
      continue;
    }

    if ((stricmp (argv[0], "-v") == 0) || (stricmp (argv[0], "--verbose") == 0)) {
      SetPrintLevel (VERBOSE_LOG_LEVEL);
      VerboseMsg ("Verbose output Mode Set!");
      argc --;
      argv ++;
      continue;
    }

    if ((stricmp (argv[0], "-q") == 0) || (stricmp (argv[0], "--quiet") == 0)) {
      SetPrintLevel (KEY_LOG_LEVEL);
      KeyMsg ("Quiet output Mode Set!");
      argc --;
      argv ++;
      continue;
    }

    if ((stricmp (argv[0], "-d") == 0) || (stricmp (argv[0], "--debug") == 0)) {
      Status = AsciiStringToUint64 (argv[1], FALSE, &LogLevel);
      if (EFI_ERROR (Status)) {
        Error (NULL, 0, 1003, "Invalid option value", "%s = %s", argv[0], argv[1]);
        goto Finish;
      }
      if (LogLevel > 9) {
        Error (NULL, 0, 1003, "Invalid option value", "Debug Level range is 0~9, currnt input level is %d", LogLevel);
        goto Finish;
      }
      SetPrintLevel (LogLevel);
      DebugMsg (NULL, 0, 9, "Debug Mode Set", "Debug Output Mode Level %s is set!", argv[1]);
      argc -= 2;
      argv += 2;
      continue;
    }

    //
    // Get Input file name
    //
    if ((InputFileNum == 0) && (InputFileName == NULL)) {
      InputFileName = (CHAR8 **) malloc (MAXIMUM_INPUT_FILE_NUM * sizeof (CHAR8 *));
      if (InputFileName == NULL) {
        Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated");
        return EFI_OUT_OF_RESOURCES;
      }

      memset (InputFileName, 0, (MAXIMUM_INPUT_FILE_NUM * sizeof (CHAR8 *)));
    } else if (InputFileNum % MAXIMUM_INPUT_FILE_NUM == 0) {
      //
      // InputFileName buffer too small, need to realloc
      //
      InputFileName = (CHAR8 **) realloc (
                                  InputFileName,
                                  (InputFileNum + MAXIMUM_INPUT_FILE_NUM) * sizeof (CHAR8 *)
                                  );

      if (InputFileName == NULL) {
        Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated");
        return EFI_OUT_OF_RESOURCES;
      }

      memset (&(InputFileName[InputFileNum]), 0, (MAXIMUM_INPUT_FILE_NUM * sizeof (CHAR8 *)));
    }

    InputFileName[InputFileNum++] = argv[0];
    argc --;
    argv ++;
  }

  VerboseMsg ("%s tool start.", UTILITY_NAME);

  //
  // Parse all command line parameters to get the corresponding section type.
  //
  VerboseMsg ("Section type is %s", SectionName);
  if (SectionName == NULL) {
    //
    // No specified Section type, default is SECTION_ALL.
    //
    SectType = EFI_SECTION_ALL;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_COMPRESSION]) == 0) {
    SectType     = EFI_SECTION_COMPRESSION;
    if (CompressionName == NULL) {
      //
      // Default is PI_STD compression algorithm.
      //
      SectCompSubType = EFI_STANDARD_COMPRESSION;
    } else if (stricmp (CompressionName, mCompressionTypeName[EFI_NOT_COMPRESSED]) == 0) {
      SectCompSubType = EFI_NOT_COMPRESSED;
    } else if (stricmp (CompressionName, mCompressionTypeName[EFI_STANDARD_COMPRESSION]) == 0) {
      SectCompSubType = EFI_STANDARD_COMPRESSION;
    } else {
      Error (NULL, 0, 1003, "Invalid option value", "--compress = %s", CompressionName);
      goto Finish;
    }
    VerboseMsg ("Compress method is %s", mCompressionTypeName [SectCompSubType]);
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_GUID_DEFINED]) == 0) {
    SectType     = EFI_SECTION_GUID_DEFINED;

    if (CompareGuid (&VendorGuid, &mZeroGuid) == 0) {
      memcpy (&VendorGuid, &mEfiCrc32SectionGuid, sizeof (EFI_GUID));
    }
    
    if (SectGuidAttribute == 0) {
      SectGuidAttribute = EFI_GUIDED_SECTION_PROCESSING_REQUIRED;
    }
    VerboseMsg ("Vendor Guid is %08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X", 
                VendorGuid.Data1,
                VendorGuid.Data2,
                VendorGuid.Data3,
                VendorGuid.Data4[0],
                VendorGuid.Data4[1],
                VendorGuid.Data4[2],
                VendorGuid.Data4[3],
                VendorGuid.Data4[4],
                VendorGuid.Data4[5],
                VendorGuid.Data4[6],
                VendorGuid.Data4[7]);
    if ((SectGuidAttribute & EFI_GUIDED_SECTION_PROCESSING_REQUIRED) != 0) {
      VerboseMsg ("Guid Attribute is %s", mGUIDedSectionAttribue[EFI_GUIDED_SECTION_PROCESSING_REQUIRED]);
    }
    if ((SectGuidAttribute & EFI_GUIDED_SECTION_AUTH_STATUS_VALID) != 0) {
      VerboseMsg ("Guid Attribute is %s", mGUIDedSectionAttribue[EFI_GUIDED_SECTION_AUTH_STATUS_VALID]);
    }
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_PE32]) == 0) {
    SectType = EFI_SECTION_PE32;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_PIC]) == 0) {
    SectType = EFI_SECTION_PIC;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_TE]) == 0) {
    SectType = EFI_SECTION_TE;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_DXE_DEPEX]) == 0) {
    SectType = EFI_SECTION_DXE_DEPEX;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_VERSION]) == 0) {
    SectType = EFI_SECTION_VERSION;
    if (VersionNumber < 0 || VersionNumber > 9999) {
      Error (NULL, 0, 1003, "Invalid option value", "%d is not in 0~9999", VersionNumber);
      goto Finish;
    }
    VerboseMsg ("Version section number is %d", VersionNumber);
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_USER_INTERFACE]) == 0) {
    SectType = EFI_SECTION_USER_INTERFACE;
    if (StringBuffer[0] == '\0') {
      Error (NULL, 0, 1001, "Missing option", "user interface string");
      goto Finish;
    }
    VerboseMsg ("UI section string name is %s", StringBuffer);
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_COMPATIBILITY16]) == 0) {
    SectType = EFI_SECTION_COMPATIBILITY16;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_FIRMWARE_VOLUME_IMAGE]) == 0) {
    SectType = EFI_SECTION_FIRMWARE_VOLUME_IMAGE;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_FREEFORM_SUBTYPE_GUID]) == 0) {
    SectType = EFI_SECTION_FREEFORM_SUBTYPE_GUID;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_RAW]) == 0) {
    SectType = EFI_SECTION_RAW;
  } else if (stricmp (SectionName, mSectionTypeName[EFI_SECTION_PEI_DEPEX]) == 0) {
    SectType = EFI_SECTION_PEI_DEPEX;
  } else {
    Error (NULL, 0, 1003, "Invalid option value", "SectionType = %s", SectionName);
    goto Finish;
  }
  
  //
  // Check whether there is input file
  //  
  if ((SectType != EFI_SECTION_VERSION) && (SectType != EFI_SECTION_USER_INTERFACE)) {
    //
    // The input file are required for other section type.
    //
    if (InputFileNum == 0) {
      Error (NULL, 0, 1001, "Missing options", "Input files");
      goto Finish;
    }
  }
  //
  // Check whether there is output file
  //
  for (Index = 0; Index < InputFileNum; Index ++) {
    VerboseMsg ("the %dth input file name is %s", Index, InputFileName[Index]);
  }
  if (OutputFileName == NULL) {
    Error (NULL, 0, 1001, "Missing options", "Output file");
    goto Finish;
    // OutFile = stdout;
  }
  VerboseMsg ("Output file name is %s", OutputFileName);

  //
  // Open output file
  //
  OutFile = fopen (OutputFileName, "wb");
  if (OutFile == NULL) {
    Error (NULL, 0, 0001, "Error opening file", OutputFileName);
    goto Finish;
  }
  
  //
  // At this point, we've fully validated the command line, and opened appropriate
  // files, so let's go and do what we've been asked to do...
  //
  //
  // Within this switch, build and write out the section header including any
  // section type specific pieces.  If there's an input file, it's tacked on later
  //
  switch (SectType) {
  case EFI_SECTION_COMPRESSION:
    Status = GenSectionCompressionSection (
              InputFileName,
              InputFileNum,
              SectCompSubType,
              OutFile
              );
    break;

  case EFI_SECTION_GUID_DEFINED:
    Status = GenSectionGuidDefinedSection (
              InputFileName,
              InputFileNum,
              &VendorGuid,
              SectGuidAttribute,
              OutFile
              );
    break;

  case EFI_SECTION_VERSION:
    CommonSect.Type = (EFI_SECTION_TYPE) SectType;

    Index           = sizeof (CommonSect);
    //
    // 2 bytes for the build number UINT16
    //
    Index += 2;
    //
    // StringBuffer is ascii.. unicode is 2X + 2 bytes for terminating unicode null.
    //
    Index += (strlen (StringBuffer) * 2) + 2;
    memcpy (&CommonSect.Size, &Index, 3);
    fwrite (&CommonSect, sizeof (CommonSect), 1, OutFile);
    fwrite (&VersionNumber, sizeof (UINT16), 1, OutFile);
    Ascii2UnicodeWriteString (StringBuffer, OutFile);
    VerboseMsg ("the size of the created section file is %d bytes", Index);
    break;

  case EFI_SECTION_USER_INTERFACE:
    CommonSect.Type = (EFI_SECTION_TYPE) SectType;
    Index           = sizeof (CommonSect);
    //
    // StringBuffer is ascii.. unicode is 2X + 2 bytes for terminating unicode null.
    //
    Index += (strlen (StringBuffer) * 2) + 2;
    memcpy (&CommonSect.Size, &Index, 3);
    fwrite (&CommonSect, sizeof (CommonSect), 1, OutFile);
    Ascii2UnicodeWriteString (StringBuffer, OutFile);
    VerboseMsg ("the size of the created section file is %d bytes", Index);
    break;

  case EFI_SECTION_ALL:
    //
    // read all input file contents into a buffer
    // first get the size of all file contents
    //
    Status = GetSectionContents (
              InputFileName,
              InputFileNum,
              FileBuffer,
              &InputLength
              );
  
    if (Status == EFI_BUFFER_TOO_SMALL) {
      FileBuffer = (UINT8 *) malloc (InputLength);
      if (FileBuffer == NULL) {
        Error (NULL, 0, 4001, "Resource", "memory cannot be allcoated");
        goto Finish;
      }
      //
      // read all input file contents into a buffer
      //
      Status = GetSectionContents (
                InputFileName,
                InputFileNum,
                FileBuffer,
                &InputLength
                );
    }
  
    if (Status == EFI_SUCCESS) {
      fwrite (FileBuffer, InputLength, 1, OutFile);
    }

    if (FileBuffer != NULL) {
      free (FileBuffer);
    }
    VerboseMsg ("the size of the created section file is %d bytes", InputLength);
    break;
  default:
    //
    // All other section types are caught by default (they're all the same)
    //
    Status = GenSectionCommonLeafSection (
              InputFileName,
              InputFileNum,
              SectType,
              OutFile
              );
    break;
  }

Finish:
  if (InputFileName != NULL) {
    free (InputFileName);
  }

  if (OutFile != NULL) {
    fclose (OutFile);
  }
  
  VerboseMsg ("%s tool done with return code is 0x%x.", UTILITY_NAME, GetUtilityStatus ());

  return GetUtilityStatus ();
}
