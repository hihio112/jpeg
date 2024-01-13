#define _CRT_SECURE_NO_WARNINGS //fopen 오류 해결
#include <stdio.h> // FILE*, fseek, fread 등 사용
#include <iostream>
#include <vector>
#include <math.h>
#include <string>
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0

using namespace std;
const double PI = 3.1415926;

typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;

typedef struct bitfiled {
    unsigned b0 : 1;
    unsigned b1 : 1;
    unsigned b2 : 1;
    unsigned b3 : 1;
    unsigned b4 : 1;
    unsigned b5 : 1;
    unsigned b6 : 1;
    unsigned b7 : 1;
};


struct RLC_data {
    int value;
    int skip;
    int size;
    string value_code;
    string run_size_code;
};

struct MBlock {
    int data[8][8];
    int zig_data[64] = {0};
    vector<RLC_data> RLC_group;
};

struct DC_data {
    string size_code;
    string value_code;
    int size;
    int value;
};

vector <DC_data> DC_datagroup;
vector<MBlock> MBgroup;
vector<int> DCgroup;

int qt[8][8] = {
    {16,11,10,16,24,40,51,61},
    {12,12,14,19,26,58,60,55},
    {14,13,16,24,40,57,69,56},
    {14,17,22,29,51,87,80,62},
    {18,22,37,56,68,109,103,77},
    {24,35,55,64,81,104,113,92},
    {49,64,78,87,103,121,120,101},
    {72,92,95,98,112,100,103,99}
};




struct node {
    int size = 0;
    int run = 0;
    int32 frequency = 0;
    string code = "";
    node* leftchild = NULL;
    node* rightchild = NULL;
};
vector<node> nodearray1;
vector<node> nodearray2;        //for comparision
vector<node> nodearray3;

void ReadImage(const char* fileName, const char* outfileName, byte** pixels, int32* width, int32* height, int32* bytesPerPixel, FILE*& imageFile, FILE*& OUT) // (파일이름, 픽셀값 담을 배열, 가로길이 값의 시작 위치, 세로길이 값의 시작 위치, 픽셀당 바이트 수 시작 위치
{
    imageFile = fopen(fileName, "rb");
    int32 dataOffset; 
    int32 LookUpTable=0; 
    fseek(imageFile, HEADER_SIZE + INFO_HEADER_SIZE-8, SEEK_SET); 
    fread(&LookUpTable, 4, 1, imageFile);
    fseek(imageFile, 0, SEEK_SET);
    OUT = fopen(outfileName, "wb");
    int header = 0;
    if (LookUpTable)
        header = HEADER_SIZE + INFO_HEADER_SIZE + 1024;
    else
        header = HEADER_SIZE + INFO_HEADER_SIZE;
    for (int i = 0; i < header; i++) 
    {
        int get = getc(imageFile);
        putc(get, OUT);
    }
    fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET); 
    fread(&dataOffset, 4, 1, imageFile); 
    fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
    fread(width, 4, 1, imageFile);
    fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
    fread(height, 4, 1, imageFile);
    int16 bitsPerPixel;
    fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
    fread(&bitsPerPixel, 2, 1, imageFile);
    *bytesPerPixel = ((int32)bitsPerPixel) / 8; 

    int paddedRowSize = (int)(4 * (float)(*width) / 4.0f) * (*bytesPerPixel); 
    int unpaddedRowSize = (*width) * (*bytesPerPixel);
    int totalSize = unpaddedRowSize * (*height);

    *pixels = new byte[totalSize];
    int i = 0;
    byte* currentRowPointer = *pixels + ((*height - 1) * unpaddedRowSize);
    for (i = 0; i < *height; i++)
    {
        fseek(imageFile, dataOffset + (i * paddedRowSize), SEEK_SET);       
        fread(currentRowPointer, 1, unpaddedRowSize, imageFile);            
        currentRowPointer -= unpaddedRowSize;
    }
    fclose(imageFile);
}

void WriteImage(byte* pixels, int32 width, int32 height, int32 bytesPerPixel, FILE*& outputFile)
{
    int paddedRowSize = (int)(4 * (float)width / 4.0f) * bytesPerPixel;
    int unpaddedRowSize = width * bytesPerPixel;
    for (int i = 0; i < height; i++)
    {
        int pixelOffset = ((height - i) - 1) * unpaddedRowSize;
        for (int i = 0; i < paddedRowSize; i++) {
            fwrite(&pixels[pixelOffset + i], 1, 1, outputFile);
        }
    }
    fclose(outputFile);
}

void readMacroBlock(byte* pixels, int32 width, int32 height, int32 bytesPerPixel, FILE*& outputFile) 
{
    int unpaddedRowSize = width * bytesPerPixel;
    int first_pt;
    MBlock mb;
    for (int colum = 0; colum < unpaddedRowSize; colum = colum + 8) {
        for (int row = 0; row < height; row = row + 8) {
            first_pt = row * unpaddedRowSize + colum;
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    mb.data[i][j] = pixels[first_pt + unpaddedRowSize * i + j];
                }
            }
            MBgroup.push_back(mb);
        }
    }
}
void DCT() {
    MBlock temp;
    double cu;
    double cv;
    for (int num = 0; num < MBgroup.size(); num++) {
        for (int i = 0; i < 8; i++) {
            if (i == 0) {
                cu = 1.0 / sqrt(2);
            }
            else {
                cu = 1;
            }
            for (int j = 0; j < 8; j++) {
                if (j == 0) {
                    cv = 1.0 / sqrt(2);
                }
                else {
                    cv = 1;
                }
                double sum = 0.0;
                for (int a = 0; a < 8; a++) {
                    for (int b = 0; b < 8; b++) {
                        //sum += cos((2 * a + 1) * i * PI / 16 + (2 * b + 1) * j * PI / 16) * (MBgroup[num].data[a][b]);
                        sum += cos((2 * a + 1) * i * PI / 16) * cos((2 * b + 1) * j * PI / 16) * (MBgroup[num].data[a][b]-128);
                    }
                }
                temp.data[i][j] = sum * cu * cv / 4;
            }
        }
        MBgroup[num] = temp;
    }
}

void quantization() {
    for (int num = 0; num < MBgroup.size(); num++) {
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                MBgroup[num].data[i][j] = MBgroup[num].data[i][j] / qt[i][j];
            }
        }
    }
}

void zigzag() {
    //0,0 1,0 0,1 0,2 1,1 2,0 3,0 2,1 1,2 0,3 순서로
    int x;
    int y;
    int mode;
    for (int num = 0; num < MBgroup.size();num++)
    {   
        x = 0;
        y = 0;
        mode = 0;
        for (int i = 0; i < 64; i++) {
            MBgroup[num].zig_data[i] = MBgroup[num].data[y][x];
            if ((x == 0) && (y == 0)) {
                x = 1;
            }
            else if (mode == 0) {
                if (y == 7) {
                    x += 1;
                    mode = 1;
                }
                else if (x == 0) {
                    y += 1;
                    mode = 1;
                }
                else {
                    x -= 1;
                    y += 1;
                }
            }
            else {
                if (x == 7) {
                    y += 1;
                    mode = 0;
                }
                else if (y == 0) {
                    x += 1;
                    mode = 0;
                }
                else {
                    y -= 1;
                    x += 1;
                }
            }
        }
    }
}

void dpcm() {
    for (int num = 0; num < MBgroup.size(); num++) {
        if (num == 0) {
            DCgroup.push_back(MBgroup[num].zig_data[0]);
        }
        else {
            DCgroup.push_back(MBgroup[num].zig_data[0] - MBgroup[num - 1].zig_data[0]);
        }
    }
}

string DC_Length_Table(int size) {
    string result;
    switch(size){
    case 0: 
        result = "00";
        break;
    case 1:
        result = "010";
        break;
    case 2:
        result = "011";
        break;
    case 3:
        result = "100";
        break;
    case 4:
        result = "101";
        break;
    case 5:
        result = "110";
        break;
    case 6:
        result = "1110";
        break;
    case 7:
        result = "11110";
        break;
    case 8:
        result = "111110";
        break;
    case 9:
        result = "1111110";
        break;
    case 10:
        result = "11111110";
        break;
    case 11:
        result = "111111110";
        break;
    }
    return result;
}

string DC_Value_Table(int value, int size) {
    int data = value;
    string result;
    int sign_flag;
    if (value >= 0) {
        sign_flag = 1;
    }
    else {
        sign_flag = 0;
    }

    if (data == 0) {
        result = "";
    }
    else {
        for (int i = size-1; i >= 0; i--) {
            int bit = (abs(data) >> i) & 1;
            if (sign_flag) {
                if (bit) {
                    result.append("1");
                }
                else {
                    result.append("0");
                }
            }
            else {
                if (bit) {
                    result.append("0");
                }
                else {
                    result.append("1");
                }
            }
        }
    }
    return result;
}

void DC_huffman() {
    int size;
    int value;
    DC_data temp;
    for (int num = 0; num < DCgroup.size(); num++) {
        value = DCgroup[num];
        if (value == 0) {
            size = 0;
        }
        else {
            size = log(abs(value)) / log(2) + 1;
        }
        temp.size = size;
        temp.value = value;
        temp.size_code = DC_Length_Table(size);
        temp.value_code = DC_Value_Table(value, size);
        DC_datagroup.push_back(temp);
    }
}

void RLC() {
    int cnt;
    RLC_data data;
    for (int num = 0; num < MBgroup.size(); num++) {
        cnt = 0;
        for (int i = 1; i < 64; i++) {
            if (cnt == 15) {
                data.skip = cnt;
                data.value = MBgroup[num].zig_data[i];
                cnt = 0;
                MBgroup[num].RLC_group.push_back(data);
            }
            else if (MBgroup[num].zig_data[i]==0) {
                if (i == 63) {
                    data.skip = cnt;
                    data.value = MBgroup[num].zig_data[i];
                    cnt = 0;
                    MBgroup[num].RLC_group.push_back(data);
                }
                else {
                    cnt += 1;
                }
            }
            else {
                data.skip = cnt;
                data.value = MBgroup[num].zig_data[i];
                cnt = 0;
                MBgroup[num].RLC_group.push_back(data);
            }
            if (i == 63) {  //end of block
                data.skip = 0;
                data.value = 0;
                MBgroup[num].RLC_group.push_back(data);
            }
        }
    }
}

string AC_Value_Table(int value, int size) {
    int data = value;
    string result;
    int sign_flag;
    if (value >= 0) {
        sign_flag = 1;
    }
    else {
        sign_flag = 0;
    }

    if (data == 0) {
        result = "";
    }
    else {
        for (int i = size - 1; i >= 0; i--) {
            int bit = (abs(data) >> i) & 1;
            if (sign_flag) {
                if (bit) {
                    result.append("1");
                }
                else {
                    result.append("0");
                }
            }
            else {
                if (bit) {
                    result.append("0");
                }
                else {
                    result.append("1");
                }
            }
        }
    }
    return result;
}

void RLC_huffman() {
    int size;
    for (int num = 0; num < MBgroup.size(); num++) {
        for (int i = 0; i < MBgroup[num].RLC_group.size(); i++)
        {
            int value = MBgroup[num].RLC_group[i].value;
            if (value == 0) {
                size = 0;
            }
            else {
                size = log(abs(value)) / log(2) + 1;
            }
            MBgroup[num].RLC_group[i].size = size;
            MBgroup[num].RLC_group[i].value_code = AC_Value_Table(value, size);
        }
    }
}//size와 value code 만들기

node extractMin()
{
    int32 min = UINT32_MAX;
    vector<node>::iterator iter, position;
    for (iter = nodearray2.begin(); iter != nodearray2.end(); iter++)
    {
        if (min > (*iter).frequency)
        {
            position = iter;
            min = (*iter).frequency;
        }
    }
    node temp = (*position);
    nodearray2.erase(position);
    return temp;
}

node getHuffmanTree()
{
    node result;
    while (!nodearray2.empty())
    {
        node* tempNode = new node;
        node* tempNode1 = new node;
        node* tempNode2 = new node;
        *tempNode1 = extractMin();
        *tempNode2 = extractMin();

        tempNode->leftchild = tempNode1;
        tempNode->rightchild = tempNode2;
        tempNode->frequency = tempNode1->frequency + tempNode2->frequency;
        nodearray2.push_back(*tempNode);

        if (nodearray2.size() == 1)
            break;
    }
    result = nodearray2[0];
    return result;
}

void depthFirstSearch(node* tempRoot, string s)
{
    node* root1 = tempRoot;
    root1->code = s;

    if (root1 == NULL)
    {

    }
    else if (root1->leftchild == NULL && root1->rightchild == NULL)
    {
        //cout << "node2추가:\t" << root1->run << "\t" << "size:\t" << root1->size << "\t" << "code:\t" << root1->code << endl;
        nodearray1.push_back(*root1);
    }
    else
    {
        root1->leftchild->code = s.append("0");
        s.erase(s.end() - 1);
        root1->rightchild->code = s.append("1");
        s.erase(s.end() - 1);

        depthFirstSearch(root1->leftchild, s.append("0"));
        s.erase(s.end() - 1);
        depthFirstSearch(root1->rightchild, s.append("1"));
        s.erase(s.end() - 1);
    }
}

void get_run_size_code() {
    for (int num = 0; num < MBgroup.size(); num++)
    {
        //cout << num << "번째 macro"<<"\n";
        //get node and frequency at nodearray1
        int flag = 0;
        for (int i = 0; i < MBgroup[num].RLC_group.size(); i++)
        {
            int l = nodearray3.size();
            int size = MBgroup[num].RLC_group[i].size;
            int run = MBgroup[num].RLC_group[i].skip;
            if (size == 0 && run == 0) {
                //end of RLC
                break;
            }
            //cout << "몇번 째 mb"<<num<<"i번째 RLC_group: "<<i<<"\n";
            if (l != 0) {
                for (int j = 0; j < l; j++) {
                    if ((nodearray3[j].size == size) && (nodearray3[j].run == run)) {
                        nodearray3[j].frequency += 1;
                        flag = 1;
                        break;
                    }
                    else if (j == l - 1) {
                        flag = 0;
                    }
                }
            }
            if (!flag) {
                node temp = {0};
                temp.size = size;
                temp.frequency = 0;
                temp.run = run;
                nodearray3.push_back(temp);
                //cout << "nodearray추가 현재 배열사이즈" << nodearray3.size() << "\n";
            }            
        }
    }
    //cout <<"node3 size" << nodearray3.size()<<"\n";
    nodearray2 = nodearray3;
    //get huffman tree 
    node root = getHuffmanTree();
    depthFirstSearch(&root, "");
    for (int num = 0; num < MBgroup.size(); num++) {
        //cout << "huffman tree"<<nodearray1.size()<<"\n";
        //copy code to RLC_group

        for (int i = 0; i < MBgroup[num].RLC_group.size(); i++) {
            int target_size = MBgroup[num].RLC_group[i].size;
            int target_run = MBgroup[num].RLC_group[i].skip;
            //cout << "copycode" << i << "\n";
            for (int j = 0; j < nodearray1.size(); j++) {
                if (nodearray1[j].size == target_size && nodearray1[j].run == target_run) {
                    MBgroup[num].RLC_group[i].run_size_code = nodearray1[j].code;
                //cout  << j<<"번째에서 발견" << "\n";
                }
            }
        }
    }
   // cout << "여기까지";
}

int main()
{
    byte* pixels;
    int32 width;
    int32 height;
    int32 bytesPerPixel;
    FILE* imageFile; 
    FILE* outputFile;
    ReadImage("HW3_Lena.bmp", "Lena_out.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile);
    
    readMacroBlock(pixels, width, height, bytesPerPixel, outputFile);
    cout << "Number of Macro-Block:\t" << MBgroup.size() << endl;

    cout << "--------------------------- READ MB완료 ---------------------------\n";
    MBlock tempMB = MBgroup[80];
    cout << "MBlock[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }
    DCT();
    cout << "--------------------------- DCT 완료 ---------------------------\n";
    tempMB = MBgroup[80];
    cout << "MBlock[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }
    quantization();
    cout << "--------------------------- quantize 완료 ---------------------------\n";
    tempMB = MBgroup[80];
    cout << "MBlock[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }
    zigzag(); 
    tempMB = MBgroup[80];
    cout << "MBlock[80]" << endl;
    for (int i = 0; i < 64; i++)
    {
        if (i == 63) {
            cout << tempMB.zig_data[i] << "\n";
        }
        else {
            cout << tempMB.zig_data[i] << ",";
        }
    }
    // DPCM
    cout << "--------------------------- DPCM ---------------------------\n";
    dpcm();
    cout << "DPCM 벡터 사이즈:\t" << DCgroup.size() << "\n";
    cout << "20 DC value:\t";
    for (int i = 0; i < 20; i++)
    {
        if (i == 19) {
            cout << MBgroup[i].zig_data[0] << "\n";
        }
        else {
            cout << MBgroup[i].zig_data[0] << ",";
        }
    }
    cout << "20 DPCM value : \t";
    for (int i = 0; i < 20; i++)
    {
        if (i == 19) {
            cout << DCgroup[i] << "\n";
        }
        else {
            cout << DCgroup[i] << ",";
        }
    }
    DC_huffman();
    cout << "20 DC information\n";
    for (int i = 0; i < 20; i++)
    {
        cout << "size:\t" << DC_datagroup[i].size << "\t" << "value:\t" << DC_datagroup[i].value << "\t" << "code:\t" << DC_datagroup[i].size_code << DC_datagroup[i].value_code << "\n";
    }
    cout << "--------------------------- AC ---------------------------\n";
    cout << "AC:\t";
    for (int i = 0; i < 63; i++)
    {
        cout << MBgroup[80].zig_data[i+1]<<", ";
    }
    RLC();
    RLC_huffman();
    cout << "\n";
    get_run_size_code();
    for (int i = 0; i < 16; i++)
    {
        cout << "runlength: "<<MBgroup[80].RLC_group[i].skip <<"\tvalue : "<< MBgroup[80].RLC_group[i].value<<"\tvalue code : " << MBgroup[80].RLC_group[i].value_code << "\trun_size code : "<< MBgroup[80].RLC_group[i].run_size_code <<"\n";
        if (MBgroup[80].RLC_group[i].skip == 0 && MBgroup[80].RLC_group[i].value==0) {
            break;
        }
    }
    string total_code = "";
    
    //DC code
    for (int i = 0; i < DC_datagroup.size(); i++)
    {
        total_code.append(DC_datagroup[i].size_code);
        total_code.append(DC_datagroup[i].value_code);
    }
    cout << "DC data: " << total_code.size() << "\n";
    //AC code
    for (int i = 0; i < MBgroup.size(); i++) {
        for (int j = 0; j < MBgroup[i].RLC_group.size(); j++) {
            total_code.append(MBgroup[i].RLC_group[j].run_size_code);
            total_code.append(MBgroup[i].RLC_group[j].value_code);
        }
        

    }
//    cout << total_code;

    cout << "--------------------------- JPEG 완료 ---------------------------\n";
    int col = width * bytesPerPixel;
    int row = height;
    int original_data = 8 * col * row;
    
    cout << "원래 데이터: " << original_data << "\n";
    cout << "압축된 데이터: " << total_code.size() << "\n";
    double ratio = (float)original_data / total_code.size();
    cout << "압축률: " << ratio<<"\n";

    FILE* o_file;
    o_file = fopen("binary.dat", "wb");
    for (int i = 0; i < total_code.size(); i = i + 8) {
        bitfiled bit;
        memset(&bit, 0, sizeof(bitfiled));
        bit.b0 = total_code[i];
        bit.b1 = total_code[i + 1];
        bit.b2 = total_code[i + 2];
        bit.b3 = total_code[i + 3];
        bit.b4 = total_code[i + 4];
        bit.b5 = total_code[i + 5];
        if (i + 5 < total_code.size()) {
            bit.b6 = total_code[i + 6];
            bit.b7 = total_code[i + 7];
        }
        unsigned int cc = (bit.b0 * 128 + bit.b1 * 64 + bit.b2 * 32 + bit.b3 * 16 + bit.b4 * 8 + bit.b5 * 4 + bit.b6 * 2 + bit.b7);
        fwrite(&cc, 1, 1, o_file);
    }
    fclose(o_file);

    
    cout << "--------------------------- Decoding 시작 ---------------------------\n";

    // IDPCM
    for (int i = 1; i < DCgroup.size(); i++) { 
        DCgroup[i] = DCgroup[i] + DCgroup[i - 1];
        MBgroup[i].zig_data[0] = DCgroup[i];
    }
    cout << "\ndecompress후 zigzag\n";
    for (int i = 0; i < 63; i++)
    {
        cout << MBgroup[80].zig_data[i + 1] << ", ";
    }
    // RLC decompression 
    for (int i = 0; i < MBgroup.size(); i++) { 
        int x = 1;
        for (int j = 0; j < MBgroup[i].RLC_group.size()-1; j++) {
            int skip = MBgroup[i].RLC_group[j].skip;
            int value = MBgroup[i].RLC_group[j].value;
            for (int k = 0; k <= skip; k++) {
                if (k == skip) {
                    MBgroup[i].zig_data[k+x] = value;
                    x += skip+1;
                }
                else {
                    MBgroup[i].zig_data[k+x] = 0;
                }
            }
        }
    }
    cout << "\ndecompress후 zigzag\n";
    for (int i = 0; i < 63; i++)
    {
        cout << MBgroup[80].zig_data[i + 1] << ", ";
    }
    //inverse zigzag
    for (int i = 0; i < MBgroup.size(); i++) {
        int x = 0;
        int y = 0;
        int mode = 1;
        for (int j = 0; j < 64; j++) {
            MBgroup[i].data[y][x] = MBgroup[i].zig_data[j];
            if (mode == 0) {
                if (y == 7) {
                    x += 1;
                    mode = 1;
                }
                else if (x == 0) {
                    y += 1;
                    mode = 1;
                }
                else {
                    x -= 1;
                    y += 1;
                }
            }
            else {
                if (x == 7) {
                    y += 1;
                    mode = 0;
                }
                else if (y == 0) {
                    x += 1;
                    mode = 0;
                }
                else {
                    y -= 1;
                    x += 1;
                }
            }
        }
    }
    cout << "\n--------------------------- inverse zigzag ---------------------------";
    tempMB = MBgroup[80];
    cout << "\nMB[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }

    //inverse quatization;
    for (int i = 0; i < MBgroup.size(); i++) {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                MBgroup[i].data[y][x] = MBgroup[i].data[y][x] * qt[y][x];
            }
        }
    }
    cout << "\n--------------------------- inverse quantization ---------------------------";
    tempMB = MBgroup[80];
    cout << "\nMB[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }
    //inverse DCT;
    for (int i = 0; i < MBgroup.size(); i++) {
        MBlock temp;
        for (int x = 0; x < 8; x++) {
            double sum;
            for (int y = 0; y < 8; y++) {
                double cu, cv;
                sum = 0;
                for (int u = 0; u < 8; u++) {
                    for (int v = 0; v < 8; v++) {
                        if (u == 0)
                            cu = 1.0 / sqrt(2);
                        else
                            cu = 1;
                        if (v == 0)
                            cv = 1.0 / sqrt(2);
                        else
                            cv = 1.0;
                        sum += cos((2 * x + 1) * u * PI / 16) * cos((2 * y + 1) * v * PI / 16) * MBgroup[i].data[u][v] *cu *cv/4;
                    }
                }
                temp.data[x][y] = (int)sum+ 128;
            }
        }
        MBgroup[i] = temp;
    }
    cout << "\n--------------------------- inverse DCT ---------------------------";
    tempMB = MBgroup[80];
    cout << "\nMB[80]" << endl;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            cout << tempMB.data[i][j] << "\t";
        }
        cout << "\n";
    }
    //reconstruction of total pixel
    for (int i = 0; i < height; i = i + 8) // Pixels Change
    {
        int start;
        int unpaddedRowSize = width * bytesPerPixel;
        for (int j = 0; j < unpaddedRowSize; j = j + 8)
        {
            start = unpaddedRowSize * j + i;
            MBlock temp = MBgroup[0];
            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 8; y++)
                {
                    pixels[start + unpaddedRowSize * y + x] = temp.data[y][x];
                }
            }
            MBgroup.erase(MBgroup.begin());
        }
    }
    WriteImage(pixels, width, height, bytesPerPixel, outputFile);
    delete[] pixels;
    return 0;
}