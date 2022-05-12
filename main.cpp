#include <iostream>
#include "string"
#include "pthread.h"
#include <unistd.h>
#include <condition_variable>

#define READER_MIN_WORK_TIME 100
#define READER_MAX_WORK_TIME 200
#define WRITER_MIN_WORK_TIME 300
#define WRITER_MAX_WORK_TIME 500
#define NUMBER_OF_WRITERS 2
#define NUMBER_OF_READERS 10
#define WORK_TIME 1000

using namespace std;

bool isWork = true; //прапор роботи програми. Поки true програма працює
bool writerWantsToWork = false;//змінна, яка показує, що письменник хоче писати.
mutex accessToWriterState;//забезпечує монопольний доступ до змінної стану письменників
mutex singleWriter;//забезпечує, що тільки один письменник буде писати
condition_variable readerWait;/*Клас condition_variable — це примітив синхронізації,
 * який можна використовувати для блокування потоку або кількох потоків одночасно,
 * поки інший потік не змінить спільну змінну (умову) і не повідомить про condition_variable.*/
mutex readersMutex;//мютекс потрібний для condition_variable
mutex console;//забезпечує монопольний доступ до консолі

class DataBase {//наш ресурс, який ділять між собою письменники та читачі
private:
    int data;//дані
public:
    DataBase() {
        data = rand() % 100;//початкове значення
        cout << "DataBase init value: " << data << endl;
    }

    int read() {
        return data;
    }

    void write(int newData) {
        this->data = newData;
    }
} dataBase;

void sleepInRange(int min, int max);//забезпечує сон потоку випадковий час на проміжку min and max

void Sleep(int time);//сон потоку у мілісекундах

void print(const string &message);//вивід інформації у консоль. Виводити одночасно може лише один потік

void printWriterResult(const string &id, const int &data);//Вивід інформації про те, що прочитав читач

int write();//запис нових даних

void setWriterState(bool state, const string &id) {
    /*встановлює стан письменника. Якщо true,
     * то письменник хоче писати і читачі мають ставати у чергу*/
    accessToWriterState.lock();
    print("Writer: " + id + " is setting state: " + to_string(state));
    //повідомляємо, який стан хоче поставити письменник
    writerWantsToWork = state;
    accessToWriterState.unlock();
}

void *writer(void *args) {
    int intValue = *(int *) args;//індекс потоку письменника integer
    srand(intValue);
    /*зерно, яке еквівалентне індексу потоку,що у свою чергу забезпечує
     * різні випадкові значення у різних потоках*/
    string id = to_string(intValue);//індекс потоку письменника текст
    int newData;
    while (isWork) {
        sleepInRange(WRITER_MIN_WORK_TIME, WRITER_MAX_WORK_TIME);
        singleWriter.lock();//закриваємо мютекс, щоб інші письменники не могли писати
        setWriterState(true, id);//ставимо прапор, що письменник хоче писати
        newData = write();//запис даних
        sleepInRange(1, WRITER_MIN_WORK_TIME / 4);
        printWriterResult(id, newData);//виводимо у консоль нові дані
        setWriterState(false, id);//ставимо прапор, що письменник НЕ хоче писати
        readerWait.notify_all();// "будимо" всіх читачів, що чекають
        singleWriter.unlock();//звільняємо мютекс, інші письменники можуть писати
    }
}

int write() {
    int newData = rand() % 100;
    dataBase.write(newData);//передаємо у нашу базу даних нове значення
    return newData;
}

void printReaderResult(const string &id, const int &data);//вивід у консоль, що прочитав читач

void printReaderWokeUp(const string &id);//повідомляємо, що читач проснувся

void printReaderWaiting(const string &id);/*повідомляємо, що читач чекає коли
 * письменник оновить значення у БД*/

void *reader(void *args) {
    string id = to_string(*(int *) args);//індекс потоку читача текст
    int data;
    while (isWork) {
        sleepInRange(READER_MIN_WORK_TIME, READER_MAX_WORK_TIME);
        accessToWriterState.lock();//закриваємо доступ до змінної, щоб прочитати стан письменника
        if (writerWantsToWork) {
            accessToWriterState.unlock();//повертаємо доступ до змінної
            printReaderWaiting(id);//повідомляємо, що читач очікує
            unique_lock<mutex> localMutex(readersMutex);
            /*Унікальний замок — це об’єкт, який керує об’єктом мьютекса з унікальним
             * правом власності в обох станах: заблокованому та розблокованому.
             * У даному коді потрібен для виклику методу wait у condition_variable*/
            readerWait.wait(localMutex);//чекаємо поки письменник не розбудить читача
            printReaderWokeUp(id);//повідомляємо, що читач проснувся
            continue;//починаємо цикл заново
        }
        accessToWriterState.unlock();//повертаємо доступ до змінної
        data = dataBase.read();//читаємо з бази даних
        printReaderResult(id, data);//виводимо у консоль, що прочитав читач
    }
}

int main() {
    pthread_t writers[NUMBER_OF_WRITERS];
    int writersID[NUMBER_OF_WRITERS];
    for (int i = 0; i < NUMBER_OF_WRITERS; i++) {
        writersID[i] = i;
        pthread_create(&writers[i], NULL, writer, &writersID[i]);
    }
    Sleep(WRITER_MIN_WORK_TIME / 2);
    pthread_t readers[NUMBER_OF_READERS];
    int readersID[NUMBER_OF_READERS];
    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        readersID[i] = i;
        pthread_create(&readers[i], NULL, reader, &readersID[i]);
    }
    Sleep(WORK_TIME);
    isWork = false;//читачі та письменники мають виходити з циклів та завершувати роботу потоків
    print("isWork == false");
    for (auto &i: readers) {
        pthread_join(i, NULL);
    }
    for (auto &i: writers) {
        pthread_join(i, NULL);
    }
    return 0;
}

void sleepInRange(int min, int max) {
    int time = rand() % (max - min + 1) + min;
    Sleep(time);
}

void Sleep(int time) {//мілісекунди
    usleep(time * 1000);// usleep працює у мікросекундах, тому ми домножуємо на 1000
}

void print(const string &message) {
    console.lock();//забираємо доступ до консолі
    cout << message << endl;
    console.unlock();//повертаємо доступ до консолі
}

void printReaderResult(const string &id, const int &data) {
    string message = "Reader: " + id + " has read a data value: " + to_string(data);
    print(message);
}

void printReaderWaiting(const string &id) {
    string message = "Reader: " + id + " is waiting";
    print(message);
}

void printReaderWokeUp(const string &id) {
    string message = "Reader: " + id + " has woken up";
    print(message);
}

void printWriterResult(const string &id, const int &data) {
    string message = "Writer: " + id + " has written a data value: " + to_string(data);
    print(message);
}