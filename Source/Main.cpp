
/*
  ==============================================================================
   JUCE BinaryBuilder-based console app; 
   Input: files with same extension in the same directory (non-recursive).
   Output: .cpp and .h with indexed data, getters and enumerators.
   Usage: fileExt[opt] srcDir[opt] destDir[opt] className[opt]
   Defaults: png \. \. BinaryData p n s
   
   Creates namespace (className) with:
    const char* getFile(int idx);           // pointer to resource; 
    const int getFileSize(int idx);         // resource size in bytes; 
    const int numFiles;                     // total number of files
    const char* getFileName(int idx);       // resource name w/o file extension
    namespace Items {enum {myImage,...};}   // names as file indices

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"

StringArray names;
Array<size_t> sizes;
size_t totalSize = 0;
ScopedPointer<OutputStream> header;
ScopedPointer<OutputStream> cpp;
String className;
//==============================================================================
static void addFile (const File& file)
{
    MemoryBlock mb;
    file.loadFileAsData (mb);

    const String name (file.getFileNameWithoutExtension()
                           .replaceCharacter (' ', '_')
                           .replaceCharacter ('.', '_')
                           .retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789"));

    std::cout << "Adding " << name << ": " << (int) mb.getSize() << " bytes" << std::endl;

    *header << "  extern const char*  " << name << "_mem;\r\n"
                    "  const int           " << name << "_size = "
                 << (int) mb.getSize() << ";\r\n\r\n";

    static int tempNum = 0;

    *cpp << "static const unsigned char temp" << ++tempNum << "[] = {";

    size_t i = 0;
    const uint8* const data = (const uint8*) mb.getData();

    while (i < mb.getSize() - 1)
    {
        if ((i % 40) != 39)
            *cpp << (int) data[i] << ",";
        else
            *cpp << (int) data[i] << ",\r\n  ";

        ++i;
    }

    *cpp << (int) data[i] << ",0,0};\r\n";

    *cpp << "const char* " << className << "::" << name
              << "_mem = (const char*) temp" << tempNum << ";\r\n\r\n";
    names.add(name);
    sizes.add(mb.getSize());
    totalSize += mb.getSize();
}

static bool isHiddenFile (const File& f, const File& root)
{
    return f.getFileName().endsWithIgnoreCase (".scc")
         || f.getFileName() == ".svn"
         || f.getFileName().startsWithChar ('.')
         || (f.getSize() == 0 && ! f.isDirectory())
         || (f.getParentDirectory() != root && isHiddenFile (f.getParentDirectory(), root));
}


//==============================================================================
int main (int argc, char* argv[]) {
  std::cout << "\r\n  JUCE BinaryBuilder-based app\r\n" 
    "Input: files with same extension and located in the same directory (non-recursive).\r\n" 
    "Output: .cpp and .h with indexed data, getters and enumerators.\r\n"
  "Usage: fileExt[opt] srcDir[opt] destDir[opt] className[opt]\r\n" 
  " Defaults are png .\\ .\\ BinaryData\r\n\r\n" ;

  if( argc > 5)   return 0;

  File sourceDirectory(File::getCurrentWorkingDirectory());
  File destDirectory(sourceDirectory);

  String fileExt;

  if (argc > 1) {
    fileExt << String(argv[1]).trim().removeCharacters(".*");
    if (argc > 2) {
      sourceDirectory = sourceDirectory.getChildFile(String(argv[2]).unquoted());
      if (!sourceDirectory.isDirectory()) {
        std::cout << "Source directory doesn't exist: " << sourceDirectory.getFullPathName()
          << std::endl << std::endl;
        return 0;
      }
      if (argc > 3) {
        destDirectory = destDirectory.getChildFile(String(argv[3]).unquoted());
        if (!destDirectory.isDirectory()) {
          std::cout << "Destination directory doesn't exist: "
            << destDirectory.getFullPathName() << std::endl << std::endl;
          return 0;
        }
        if (argc > 4) className = String(argv[3]).trim();
      }
    }
  }
  if (fileExt.isEmpty()) fileExt = "png";
  if (className.isEmpty()) className = "BinaryData";

  const File headerFile (destDirectory.getChildFile (className).withFileExtension (".h"));
  const File cppFile    (destDirectory.getChildFile (className).withFileExtension (".cpp"));

  std::cout << "Creating " << headerFile.getFullPathName()
            << " and " << cppFile.getFullPathName()
            << " from files in " << sourceDirectory.getFullPathName()
            << "..." << std::endl << std::endl;

  Array<File> files;
  sourceDirectory.findChildFiles (files, File::findFiles, false, String("*.") + fileExt);

  if (files.size() == 0) {
      std::cout << "Didn't find any source files in: " 
        << sourceDirectory.getFullPathName() << std::endl << std::endl;
      return 0;
  }

  headerFile.deleteFile();
  cppFile.deleteFile();

  header  = headerFile.createOutputStream();

  if (header == nullptr) {
      std::cout << "Couldn't open " << headerFile.getFullPathName() << " for writing" << std::endl << std::endl;
      return 0;
  }

  cpp  = cppFile.createOutputStream();

  if (cpp == nullptr) {
  std::cout << "Couldn't open " << cppFile.getFullPathName() << " for writing" << std::endl << std::endl;
      return 0;
  }

  *header << "/* (Auto-generated binary data file). */\r\n\r\n"
              "#pragma once\r\n\r\n"
              "namespace " << className << " {\r\n";

  *cpp << "/* (Auto-generated binary data file). */\r\n\r\n"
          "#include \"" << className << ".h\"\r\n\r\n";

  for (int i = 0; i < files.size(); ++i) {
      const File file (files[i]);
      // (avoid source control files and hidden files..)
      if (! isHiddenFile (file, sourceDirectory)) addFile(file);
  }

  *header << "  extern const char* getFile(int i);\r\n";
  *header << "  extern const size_t getFileSize(int i);\r\n";
  *header << "  const int numFiles = " << names.size() << ";\r\n";
  *header << "  extern const char* getFileName(int i);\r\n\r\n";

  *cpp << "static const char* temp_ptrs[] = {\r\n";

  for (String* p = names.begin();;) {
    *cpp << "  " << className << "::" << (*p) << "_mem";
    if ((++p) == names.end()) break;
    *cpp << ",\r\n";
  }
  *cpp << "\r\n};\r\n\r\n";
  *cpp << "const char* " << className << "::getFile(int i) { return temp_ptrs[i]; }\r\n\r\n";

  *cpp << "static const size_t temp_sizes[] = {\r\n";

  for (size_t* p = sizes.begin();;) {
    *cpp << "  " << (int64_t)(*p);
    if ((++p) == sizes.end()) break;
    *cpp << ",\r\n";
  }

  *cpp << "\r\n};\r\n\r\n";
  *cpp << "const size_t " << className << "::getFileSize(int i) { return temp_sizes[i]; }\r\n\r\n";

  *cpp << "static const char* temp_names[] = {\r\n";

  for (String* p = names.begin();;) {
    *cpp << "  \"" << *p << "\"";
    if ((++p) == names.end()) break;
    *cpp << ",\r\n"; 
  }
  *cpp << "\r\n};\r\n\r\n";
  *cpp << "const char* " << className << "::getFileName(int i) { return temp_names[i]; }\r\n\r\n";
  

  *header << "  namespace Items {\r\n    enum: int{\r\n";
  for (String* p = names.begin();;) {
    *header << "      " << *p;
    if ((++p) == names.end()) break;
    *header << ",\r\n"; 
  }
  *header << "\r\n    };\r\n  }\r\n\r\n}\r\n";

  header = nullptr;
  cpp = nullptr;

  std::cout << std::endl << " Total size of binary data: " << totalSize << " bytes" << std::endl;

  return 0;
}
