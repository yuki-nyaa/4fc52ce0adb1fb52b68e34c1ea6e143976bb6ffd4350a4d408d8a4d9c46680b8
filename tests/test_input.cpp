#include<reflex/input.h>
#include<fstream>
void to_utf32be(char32_t c,char* p){
    p[0] = (c >> 24) & 0xFF;
    p[1] = (c >> 16) & 0xFF;
    p[2] = (c >> 8) & 0xFF;
    p[3] = c & 0xFF;
}
int main(){
    using namespace reflex;

    printf("TEST 1\n");
    FILE* f1 = fopen("test_input_1.txt","r");
    Input input1(f1,Input::encoding::utf8);
    char arr1[1024];
    size_t arr1_s = input1.get_raw(arr1,1,1024);
    for(size_t i=0;i<arr1_s;++i)
        printf("%x ",static_cast<unsigned char>(arr1[i]));
    fclose(f1);
    input1.set_source();

    printf("\n\n");

    printf("TEST 2\n");
    f1 = fopen("test_input_2.txt","r");
    input1.set_source(f1);
    input1.set_encoding(Input::encoding::utf16le);
    char arr2[1024];
    size_t arr2_s = 0;
    {
    char * p = arr2;
    size_t i;
    while((i = input1.get(p))>0){
        for(size_t j=0;j<i;++j)
            printf("%x ",static_cast<unsigned char>(p[j]));
        p+=i;
        arr2_s+=i;
    }
    }
    fclose(f1);
    input1.set_source();

    printf("\n\n");

    printf("TEST 3\n");
    char32_t u32arr1[1024];
    size_t u32arr1_s = 0;
    {
    char* p = arr2;
    char* p2;
    char32_t* u32p = u32arr1;
    while(*u32p=from_utf8(p,&p2)){
        printf("%x ",*u32p++);
        p=p2;
        ++u32arr1_s;
    }
    }

    printf("\n\n");

    printf("TEST 4\n");
    char arr3[1024];
    size_t arr3_s = 0;
    {
    char* p = arr3;
    for(size_t i=0;i<u32arr1_s;++i){
        to_utf32be(u32arr1[i],p);
        p+=4;
        arr3_s+=4;
    }
    }
    for(size_t i=0;i<arr3_s;++i)
        printf("%x ",static_cast<unsigned char>(arr3[i]));

    printf("\n\n");

    printf("TEST 5\n");
    input1.set_source(arr3,arr3_s);
    input1.set_encoding(Input::encoding::utf32be);
    char arr4[1024];
    size_t arr4_s = input1.get_raw(arr4,1,1024);
    for(size_t i=0;i<arr4_s;++i)
        printf("%x ",static_cast<unsigned char>(arr4[i]));
    input1.set_source();

    printf("\n\n");

    printf("TEST 6\n");
    input1.set_source(arr3,arr3_s);
    input1.set_encoding(Input::encoding::utf32be);
    char arr5[1024];
    size_t arr5_s = 0;
    {
    char* p = arr5;
    size_t i;
    while((i = input1.get(p))>0){
        for(size_t j=0;j<i;++j)
            printf("%x ",static_cast<unsigned char>(p[j]));
        p+=i;
        arr5_s+=i;
    }
    }
    input1.set_source();

    printf("\n\n");

    printf("TEST7\n");
    char arr6[1024];
    f1 = fopen("test_input_2.txt","r");
    input1.set_source(f1);
    input1.set_encoding(Input::encoding::utf16le);
    size_t arr6_s = input1.get_raw(arr6,1,1024);
    fclose(f1);
    input1.set_source(arr6);
    char arr7[1024];
    size_t arr7_s = 0;
    // Note: because utf-16 `\r` and `\n` contain zero byte, the following test should only print a single line.
    {
    char* p = arr7;
    size_t i;
    while((i = input1.get(p))>0){
        for(size_t j=0;j<i;++j)
            printf("%x ",static_cast<unsigned char>(p[j]));
        p+=i;
        arr7_s+=i;
    }
    }
    input1.set_source();

    printf("\n\n");

    printf("TEST8\n");
    std::ifstream fs1("test_input_2.txt");
    input1.set_source(fs1);
    input1.set_encoding(Input::encoding::utf16le);
    char arr8[1024];
    size_t arr8_s = input1.get_raw(arr8,1,1024);
    for(size_t i=0;i<arr8_s;++i)
        printf("%x ",static_cast<unsigned char>(arr8[i]));
    printf("\n");
    fs1.close();
    fs1.open("test_input_2.txt");
    char arr9[1024];
    size_t arr9_s = 0;
    {
    char* p = arr9;
    size_t i;
    while((i = input1.get(p))>0){
        for(size_t j=0;j<i;++j)
            printf("%x ",static_cast<unsigned char>(p[j]));
        p+=i;
        arr9_s+=i;
    }
    }
    input1.set_source();

    printf("\n\n");

    printf("TEST DONE!");
}