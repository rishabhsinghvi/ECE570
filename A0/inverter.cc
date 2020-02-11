#include<iostream>
#include<set>
#include<map>
#include<vector>
#include<string>
#include<fstream>
#include<cctype>


bool fileExists(const std::string& fileName)
{
    std::ifstream file(fileName.c_str());
    return file.good();
}

void readFile(const std::string& fileName, std::map<std::string, std::set<int> >& fileIndex, int index)
{
    std::ifstream file(fileName.c_str());
    if(!file)
    {
        //std::cout << "Unable to read file: " << fileName << '\n';
        return;
    }    

    char c; 
    std::string word;

    std::vector<std::string> vec;

    while(file >> std::noskipws >> c)
    {
        if(::isalpha(c))
        {
            word.push_back(c);
        }
        else
        {
            if(!word.empty())
            {
                vec.push_back(word);
            }
            word.clear();
        }
    }

    for(int i = 0 ; i < vec.size(); i++)
    {
        fileIndex[vec[i]].insert(index);
    }
}

std::vector<std::string> getFiles(const std::string& fileName)
{
    std::ifstream file(fileName.c_str());

    std::vector<std::string> fileNames;

    if(!file)
    {
        //std::cout << "Cannot open file: " << fileName << '\n';
        return fileNames;
    }

    std::string f;

    while(file >> f)
    {
        if(fileExists(f))
            fileNames.push_back(f);
    }

    return fileNames;
}

void printFileIndex(const std::map<std::string, std::set<int> >& fileIndex)
{
    
    int count = 0;
    for(std::map<std::string, std::set<int> >::const_iterator x = fileIndex.begin(); x != fileIndex.end(); x++)
    {
        std::cout << x->first << ": ";
        int inner_count = 0;
        for(std::set<int>::const_iterator y = x->second.begin(); y != x->second.end(); y++)
        {
            std::cout << *y;
            inner_count += 1;
            if(inner_count != x->second.size())
            {
                std::cout << " ";
            }

        }
        count += 1;

        if(count != fileIndex.size())
        {
            std::cout << '\n';
        }
    }

}


int main(int argc, char** argv)
{
    if(argc!=2)
    {
        //std::cout << "Usage: ./inverter <fileName>\n";
        return -1;
    }

    std::string fileName = std::string(argv[1]);

    std::map<std::string, std::set<int> > fileIndex;

    std::vector<std::string> fileNames = getFiles(fileName);

    for(unsigned int i = 0; i < fileNames.size(); i++)
    {
        readFile(fileNames[i], fileIndex, i);
    }

    printFileIndex(fileIndex);

}






















