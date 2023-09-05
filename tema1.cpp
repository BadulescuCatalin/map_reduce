#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <math.h>
#include <set>
#include <unordered_set>
#include <limits.h>

using namespace std;

//mutex folosit pt thread-urile de mappers, am o variabila care imi spune ce fisier trebuie citit
pthread_mutex_t mutexCitire;
//mutex folosit pt a adauga nr in set-ul partajat de thread-uri
pthread_mutex_t mutexAdd;
//bariera care le da start-ul thread-urilor de reducer
pthread_barrier_t barrier;

//in structura pentru mappers am un pointer de set care imi spune ce numere am verificat deja(comun la toate thread-urile)
//am numarul de fisiere ce trebuiesc citite, path-ul si un pointer de int ce imi spune la ce fisier am ramas(comun la toate thread-urile)
//perfectPowers este un vector de vector de vector de int care imi spune: id-ul thread-ului, exponentul si lista de numere care se pot scrie folosind exponentul
struct dataMapperi
{
    unordered_set<int> *un_s;
    int nrMapperi;
    int nrReduceri;
    int numOfFiles;
    int *fileNumber;
    string path;
    vector<string> fiesToRead;
    vector<vector<vector<int>>> *perfectPowers;
    int id;
};


//perfectPowers este un pointer la care poinmteaza la acelasi vector ca mai sus
//un_s pointeaza la acelasi set ca la mapperi
struct dataReducers
{
    vector<vector<vector<int>>> *perfectPowers;
    int nrMapperi;
    int nrReduceri;
    int id;
    unordered_set<int> *un_s;
};


//functia folosita de mapperi
void *f(void *arg)
{
    dataMapperi argg = *(dataMapperi *)arg;
    
    //citesc fisierele pe rand, iar cand un thread este liber, citeste urmatorul fisier
    while (*(argg.fileNumber) < argg.numOfFiles)
    {
        // thread-urile trebuie sa verifice pe rand daca mai sunt fisiere de citit
        pthread_mutex_lock(&mutexCitire);
        if (*(argg.fileNumber) >= argg.numOfFiles)
        {
            pthread_mutex_unlock(&mutexCitire);
            break;
        }
        FILE *ptr;
        ptr = fopen(argg.fiesToRead[*(argg.fileNumber)].c_str(),"r");
        // incrementez nr fisierului urmator
        *(argg.fileNumber) += 1;
        pthread_mutex_unlock(&mutexCitire);
        int n;
        fscanf(ptr, "%d", &n);
        for (int i = 0; i < n; ++i)
        {
            // citesc numerele din fisier, si le pun in set in cazul in care nu am mai avut si inainte/ pe al thread acelasi numar
            int x;
            fscanf(ptr, "%d", &x);
            if ((*argg.un_s).find(x) == (*argg.un_s).end())
            {
                pthread_mutex_lock(&mutexAdd);
                (*argg.un_s).insert(x);
                pthread_mutex_unlock(&mutexAdd);
                // daca numarul citit e 1, atunci il pun la lista fiecarui exponent din, in thread-ul curent
                if (x == 1)
                {
                    for (int exp = 2; exp <= argg.nrReduceri + 1; ++exp)
                    {
                        //argg.id = id-ul thread-ului
                        (*argg.perfectPowers)[argg.id][exp - 2].push_back(1);
                    }
                }
                // daca am citit un numar care nu e 1, verific daca se poate scrie ca a ^ exp
                // pe a il caut binar in functie de exp
                else
                {
                    for (int exp = 2; exp <= argg.nrReduceri + 1; ++exp)
                    {
                        int low = 1;
                        int high = sqrt(x);

                        while (low <= high)
                        {
                            int mid = low + (high - low) / 2;
                            
                            if (exp * log(mid) <= log(INT_MAX) && pow(mid, exp) == x)
                            {
                                (*argg.perfectPowers)[argg.id][exp - 2].push_back(x);
                                break;
                            }
                            if (exp * log(mid) <= log(INT_MAX) && pow(mid, exp) < x)
                            {
                                low = mid + 1;
                            }
                            else
                            {
                                high = mid - 1;
                            }
                        }
                    }
                }
            }
        }
        fclose(ptr);
    }
    // cresc contorul barierei, pentru a stii cand pornesc thread-urile de reducers
    pthread_barrier_wait(&barrier);
    pthread_exit(NULL);
}


//functia folosita la thread-urile de reducers
void *fReducers(void *arg)
{
    // anunt ca am ajuns la bariera
    pthread_barrier_wait(&barrier);
    dataReducers argg = *(dataReducers *)arg;

    string idString = to_string(argg.id + 2);
    string outFile = "out" + idString + ".txt";
    int num = 0;
    // pentru ca numerele ce ajung in lista sunt unice datorita set-ului partajat de thread-urile de mapperi
    // pot calcula numarul de puteri unice ca suma dimensiunilor listelor fiecarui exponent al fiecarui thread in parte
    for (int i = 0; i < argg.nrMapperi; ++i)
    {
        //argg.id este exponentul de care se ocupa thread-ul respectiv
        int size = (*argg.perfectPowers)[i][argg.id].size();
        num += size;
    }
    ofstream fout(outFile);
    fout << num;
    fout.close();
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    int i, r;
    int mapperi, reduceri;

    mapperi = atoi(argv[1]);
    reduceri = atoi(argv[2]);

    void *status;

    pthread_t threadsRecuceri[reduceri];
    pthread_t threadsMapperi[mapperi];

    dataMapperi argumentsMappers[mapperi];
    dataReducers argumentsReducers[reduceri];

    pthread_mutex_init(&mutexCitire, NULL);
    pthread_mutex_init(&mutexAdd, NULL);
    pthread_barrier_init(&barrier, NULL, mapperi + reduceri);

    int a = 0;

    unordered_set<int> set;
    //initializez structurile de mappers si reducers
    for (int i = 0; i < mapperi; ++i)
    {
        argumentsMappers[i].fileNumber = &a;
        argumentsMappers[i].path = string(argv[3]);
        argumentsMappers[i].nrMapperi = mapperi;
        argumentsMappers[i].nrReduceri = reduceri;
        argumentsMappers[i].un_s = &set;
    }
    for (int i = 0; i < reduceri; ++i)
    {
        argumentsReducers[i].un_s = &set;
        argumentsReducers[i].nrMapperi = mapperi;
        argumentsReducers[i].nrReduceri = reduceri;
    }

    //vecotrul spre care pointeaza perfectPowers din mappers si reducers
    vector<vector<vector<int>>> vvv;
    for (int j = 0; j < mapperi; ++j)
    {
        vector<vector<int>> vv;
        for (int i = 0; i < reduceri; ++i)
        {
            vector<int> v;
            vv.push_back(v);
        }
        vvv.push_back(vv);
    }

    argumentsMappers[0].perfectPowers = &vvv;
    for (int i = 1; i < mapperi; ++i)
    {
        argumentsMappers[i].perfectPowers = argumentsMappers[0].perfectPowers;
    }

    for (int i = 0; i < reduceri; ++i)
    {
        argumentsReducers[i].perfectPowers = argumentsMappers[0].perfectPowers;
    }

    ifstream fin(argumentsMappers[0].path);
    int n;
    fin >> n;
    for (int i = 0; i < mapperi; ++i)
    {
        argumentsMappers[i].numOfFiles = n;
    }
    
    for (int i = 0; i < n; ++i)
    {
        string s;
        fin >> s;
        for (int j = 0; j < mapperi; ++j)
        {
            argumentsMappers[j].fiesToRead.push_back(s);
        }
    }

    //Pornesc thread-urile de mapperi si le dau id-uri
    for (i = 0; i < mapperi; i++)
    {
        argumentsMappers[i].id = i;
        r = pthread_create(&threadsMapperi[i], NULL, f, &argumentsMappers[i]);

        if (r)
        {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
    }
    
    //Pornesc thread-urile de reduceri si le dau id-uri
    for (i = 0; i < reduceri; i++)
    {
        argumentsReducers[i].id = i;
        r = pthread_create(&threadsRecuceri[i], NULL, fReducers, &argumentsReducers[i]);

        if (r)
        {
            printf("Eroare la crearea thread-ului %d\n", i);
            exit(-1);
        }
    }

    for (i = 0; i < mapperi; i++)
    {
        r = pthread_join(threadsMapperi[i], &status);

        if (r)
        {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }

    for (i = 0; i < reduceri; i++)
    {
        r = pthread_join(threadsRecuceri[i], &status);

        if (r)
        {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }
    pthread_mutex_destroy(&mutexCitire);
    pthread_mutex_destroy(&mutexAdd);
    pthread_barrier_destroy(&barrier);

    return 0;
}