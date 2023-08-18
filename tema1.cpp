#include <pthread.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <bits/stdc++.h>

using namespace std;

struct argument {
    // pentru salvarea id-ului threadului
    int id;
    // pentru salvarea fisierelor de intrare
    stack<string> *input_files;
    // lista pentru mapperi
    vector<vector<vector<long>>> *lists;
    pthread_barrier_t *barrier;
    pthread_mutex_t *mutex;
    int nr_mappers;
    int nr_reducers;
    // procesare pt reduceri
    unordered_map<long, bool> uniq;
    int exponent;
    // functie pt mapper/reducer
    void *(*func)(void * arg);
};

// cautare binara pentru a verifica daca un element e
// putere perfecta
bool binary_search(long l, long r, long n, int exp) {
    while (r >= l) {
        long mid = l + (r - l) / 2;
        long long nr = pow (mid, exp);
        if (nr == n) {
            return true;
        }

        if (nr > n) {
            r = mid - 1;
        } else {
            l = mid + 1;
        }
    }
    return false;
}

void verify_element(long n, int nr_reducers, vector<vector<long>> &list) {
    // verific pt fiecare exponent
    for (int i = 2; i <= nr_reducers + 1; i++) {
        long l = 1;
        long r = sqrt(n);
        bool is_p;

        is_p = binary_search(l, r, n, i);

        // daca este putere perfecta il adaug
        // in lista mapperului la exponentul
        // aferent
        if (is_p) {
            list[i].push_back(n);
        }
    }
}

void *mapper_func(void *arg) {
    int nr_reducers = ((argument *)arg)->nr_reducers;
    int id = ((argument *)arg)->id;

    long size;
    long nr;

    while (1) {
        // lock pt a putea scoate din stiva fara
        // intercalarea threadurilor
        pthread_mutex_lock(((argument *)arg)->mutex);
        // daca stiva nu mai are elemente
        // se opreste bucla si se da unlock
        if ((*((argument *)arg)->input_files).size() == 0) {
                pthread_mutex_unlock(((argument *)arg)->mutex);
                break;
        } else {
            // salvez primul fisier de pe stiva
            string file;
            file = (*((argument *)arg)->input_files).top();
            cout << "threadId:" << id << "   " << file << endl;
            // scot fisierul de pe stiva
            (*((argument *)arg)->input_files).pop();
            // unlock pt a-si putea lua fisier de pe stiva
            // urmatorul thread
            pthread_mutex_unlock(((argument *)arg)->mutex);

            // citesc elementele din fisier si verific
            // daca sunt puteri perfecte, adaugandu-le
            // la mapper
            ifstream input_file;
            input_file.open(file);
            input_file >> size;

            for (long i = 0; i < size; i++) {
                input_file >> nr;
                verify_element(nr, nr_reducers, (*((argument *)arg)->lists)[id]);
            }

            input_file.close();
        }
    }
    // bariera pt ca reducerii sa inceapa procesarea
    // dupa ce mapperi termina citirea
    pthread_barrier_wait(((argument *)arg)->barrier);

    return NULL;
}

void *reducer_func(void *arg) {
    pthread_barrier_wait(((argument *)arg)->barrier);

    // pentru fiecare mapper
    for (long unsigned int i = 0; i < (*((argument *)arg)->lists).size(); i++) {
        // iterez prin lista exponentului potrivit reducerului la care ma aflu
        for (long unsigned int j = 0; j < (*((argument *)arg)->lists)[i][((argument *)arg)->exponent].size(); j++) {
            long elem = (*((argument *)arg)->lists)[i]
            [((argument *)arg)->exponent][j];
            // daca elementul nu a mai aparut il adaug in map
            if (((argument *)arg)->uniq.find(elem) ==
            ((argument *)arg)->uniq.end()) {
                    ((argument *)arg)->uniq[elem] = true;
            }
        }
    }

    ofstream output_file;
    // creez fisierul de output
    string output_filename = "out" + to_string(((argument *)arg)->exponent) + ".txt";

    output_file.open(output_filename);
    // scriu numarul de aparitii unice
    output_file << ((argument *)arg)->uniq.size();

    output_file.close();

    return NULL;
}

void *f(void *arg) {
    // apelez functia aferenta mapper/ reducer
    ((argument *)arg)->func(arg);
  	pthread_exit(NULL);
}

int main(int argc, char** argv) {
    ifstream main_input_file;
    string input_filename;
    long nr_files;
    int nr_mappers;
    int nr_reducers;
    //int r;
    int m;
    int id;
    void *status;
    stack<string> input_files;
    pthread_barrier_t barrier;
    pthread_mutex_t mutex;

    if (argc < 3) {
        perror("Format: <numar_mapperi> <numar_reduceri> <fisier_intrare>\n");
        exit(-1);
    }
    // extrag datele
    nr_mappers = atoi(argv[1]);
    nr_reducers =  atoi(argv[2]);
    input_filename = argv[3];


    main_input_file.open(input_filename);
    main_input_file >> nr_files;

    // citesc din fisier fisierele si le salvez pe stiva
    for (long i = 0; i < nr_files; i++) {
        string temp;
        main_input_file >> temp;
        input_files.push(temp);
    }

    main_input_file.close();


    pthread_t threads[nr_mappers + nr_reducers];
    // structura pt fiecare thread
    vector<argument> arguments(nr_mappers + nr_reducers);
    // fiecare mapper are un vector (exponent) de numere
    vector<vector<vector<long>>> lists;

    for (int i = 0; i < nr_mappers; i++) {
        vector<vector<long>> temp(nr_reducers + 2);
        lists.push_back(temp);
    }

    // initializare bariera
    pthread_barrier_init(&barrier, NULL, nr_mappers + nr_reducers);
    int exp = 2;
    pthread_mutex_init(&mutex, NULL);

    for (id = 0; id < nr_mappers + nr_reducers; id++) {

        if (id < nr_mappers) {
            // pt mapperi
            arguments[id].input_files = &input_files;
            arguments[id].func = &mapper_func;
        } else {
            // pt reduceri
            arguments[id].exponent = exp;
            arguments[id].func = &reducer_func;
            exp++;
        }

        arguments[id].lists = &lists;
        arguments[id].id = id;
        arguments[id].barrier = &barrier;
        arguments[id].mutex = &mutex;
        arguments[id].nr_mappers = nr_mappers;
        arguments[id].nr_reducers = nr_reducers;

        m = pthread_create(&threads[id], NULL, f, (void *) &arguments[id]);

        if (m) {
            printf("Eroare la crearea mapper-ului %d\n", id);
            exit(-1);
        }
    }
 
    for (id = 0; id < nr_mappers + nr_reducers; id++) {
        m = pthread_join(threads[id], &status);

        if (m) {
            printf("Eroare la join mapper-ului %d\n", id);
            exit(-1);
        }
    }

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&mutex);

    return 0;
}