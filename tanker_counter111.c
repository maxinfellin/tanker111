#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lodepng.h"

typedef struct {
    int x, y;} Point;

// загрузка PNG изображения в формате RGBA
unsigned char* load_png(const char* filename, unsigned* w, unsigned* h) {
    unsigned char* img = NULL;
    if (lodepng_decode32_file(&img, w, h, filename)) return NULL;  // ошибка при декодировании
    return img;}

//сохранение изображения в PNG файл
void save_png(const char* filename, unsigned char* img, unsigned w, unsigned h) {
    unsigned char* png;//буфер для сжатого пнг
    size_t size;// размер сжатых данных
    lodepng_encode32(&png, &size, img, w, h);// кодируем ргба в пнг
    lodepng_save_file(png, size, filename);}//на диск

//ргба в оттенки серого
void to_gray(unsigned char* rgba, unsigned char* gray, int w, int h) {
    for (int i = 0; i < w*h; i++) {
        gray[i] = 0.299*rgba[4*i] +//красный канал
                  0.587*rgba[4*i+1] +//зеленый канал
                  0.114*rgba[4*i+2];}} //синий канал

//эрозия
void erosion(unsigned char* in, unsigned char* out, int w, int h, int r) {
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int min=255;// начальное макс значение
        for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++) {
            int nx=x+dx, ny=y+dy;
            if(nx>=0&&nx<w&&ny>=0&&ny<h) {
                int v=in[ny*w+nx];//проверка границ
                if(v<min) min=v;}}//мин
        out[y*w+x]=min;}} //минимум в выходной пиксель

//дилатация
void dilation(unsigned char* in, unsigned char* out, int w, int h, int r) {
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int max=0;//начальное мин значение
        for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++) {
            int nx=x+dx, ny=y+dy;
            if(nx>=0&&nx<w&&ny>=0&&ny<h) {
                int v=in[ny*w+nx];
                if(v>max) max=v;}} //находим макс
        out[y*w+x]=max;}}//максимум в выходной пиксель

// топхэт преобразование- исходное изображение минус морфологическое открытие
void top_hat(unsigned char* in, unsigned char* out, int w, int h, int r) {
    unsigned char* tmp = malloc(w*h);//буфер для эрозии
    unsigned char* opened = malloc(w*h);//для открытия

    erosion(in, tmp, w, h, r);//сначала эрозия
    dilation(tmp, opened, w, h, r);//дилатация = открытие

    for(int i=0;i<w*h;i++){
        int v = in[i] - opened[i];// вычитаем открытие из орига
        if(v < 0) v = 0;// отсекаем отриц значения
        out[i] = v;}}

// проверка, является ли пиксель водой (по ср яркости в окрестности)
int is_water(unsigned char* gray, int x, int y, int w, int h) {
    int r = 5;//радиус окрти
    int sum=0, cnt=0;

    for(int dy=-r;dy<=r;dy++){
        for(int dx=-r;dx<=r;dx++){
            int nx=x+dx, ny=y+dy;
            if(nx>=0&&nx<w&&ny>=0&&ny<h){
                sum += gray[ny*w+nx];
                cnt++;  }}}

    int mean = sum / cnt;//ср яркость
    return (mean < 95);}// вода темнее порога (баланс между берегом и морем)


int bfs(unsigned char* bin, unsigned char* gray, int* used,
        int w, int h, int sx, int sy,
        int* cx, int* cy) {
    int *q = malloc(w*h*sizeof(int));
    int head=0, tail=0;

    int sumx=0, sumy=0, count=0;// для цм
    int water=0;//счетчик водных пикселей

    q[tail++] = sy*w + sx;//  старт т в очередь
    used[sy*w + sx] = 1;// отмечаем использованной

    while(head<tail){
        int id=q[head++];
        int x=id%w, y=id/w;
        sumx+=x;
        sumy+=y;
        count++;

        if(is_water(gray,x,y,w,h)) water++;

        for(int dy=-1;dy<=1;dy++){//4 соседа крестом
            for(int dx=-1;dx<=1;dx++){
                if(dx==0&&dy==0) continue; //пропускаем текущий

                int nx=x+dx, ny=y+dy;
                if(nx>=0&&nx<w&&ny>=0&&ny<h){
                    int nid=ny*w+nx;

                    if(!used[nid] && bin[nid]){//не посещен+принадлежит пятну
                        used[nid]=1;
                        q[tail++]=nid;}}}}}

    *cx = sumx / count;// коорд х цм
    *cy = sumy / count;// коорд у цм

    if(water < count * 0.55) return 0;//танкер если 55% + - вода
    return count;}

// детектирование танкеров на бинарном изобр
int detect(unsigned char* bin, unsigned char* gray, int w, int h, Point* pts) {
    int *used = calloc(w*h, sizeof(int)); //массив посещенных пикселей
    int cnt = 0;

    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            int id=y*w+x;
            if(bin[id] && !used[id]){ //пиксель принадлежит пятну+не посещен
                int cx,cy;
                int size = bfs(bin,gray,used,w,h,x,y,&cx,&cy);

                if(size >= 3 && size <= 45){ // фильтруем по размеру (танкеры 3-45 пикселей)
                    if(is_water(gray,cx,cy,w,h) &&// доп проверка: центр и окрть ддб водой
                       is_water(gray,cx+3,cy,w,h) &&
                       is_water(gray,cx-3,cy,w,h) &&
                       is_water(gray,cx,cy+3,w,h)) {
                        pts[cnt++] = (Point){cx,cy};  }}}}}

    return cnt;}


//красные кружки на изображении в местах танкеров
void draw(unsigned char* img, Point* pts, int n, int w, int h) {
    for(int i=0;i<n;i++){
        int x=pts[i].x;
        int y=pts[i].y;

        for(int dy=-4;dy<=4;dy++){
            for(int dx=-4;dx<=4;dx++){
                if(dx*dx+dy*dy<=16){ //круг радиусом 4 пикселя
                    int nx=x+dx, ny=y+dy;
                    if(nx>=0&&nx<w&&ny>=0&&ny<h){
                        int id=(ny*w+nx)*4; // индекс в ргба массиве
                        img[id]=255;//кр канал
                        img[id+1]=0;//зел канал
                        img[id+2]=0;}}}}}}//син канал (красный цвет)

int main(int argc,char**argv){
    const char* input="image.png";
    const char* output="result.png";

    if(argc>1) input=argv[1];
    if(argc>2) output=argv[2];

    unsigned w,h;
    unsigned char* img=load_png(input,&w,&h);
    if(!img) return 1;

    unsigned char *gray=malloc(w*h);//массив для оттенков серого
    unsigned char *tophat_img=malloc(w*h);//для топхэт преобразования
    unsigned char *bin=malloc(w*h);// бинарное изобр-ие

    to_gray(img,gray,w,h);//конвертируем в оттенки серого
    top_hat(gray,tophat_img,w,h,5);//топхэт для выделения светлых объектов

    int threshold = 17;// порог бинарнизации
    for(int i=0;i<w*h;i++) {
        if (tophat_img[i] > threshold)//ярче порога
            bin[i] = 255;// белый
        else
            bin[i] = 0;}// черный

    Point *pts=malloc(10000*sizeof(Point));//для коорд танкеров
    int n = detect(bin,gray,w,h,pts);// детектируем танкеры

    printf("Tankers: %d\n",n);
    draw(img,pts,n,w,h);
    save_png(output,img,w,h);
    return 0;}