#pragma warning(disable: 4996)

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
#include<condition_variable>


using namespace std;
using namespace chrono;
using namespace literals::chrono_literals;//s, h, min, ms, us, ns

class Timer {
public:
    chrono::system_clock::time_point Begin;
    chrono::system_clock::time_point End;
    chrono::system_clock::duration RunTime;

    Timer() {//constructor
        Begin = chrono::system_clock::now();
    }

    ~Timer() {
        End = chrono::system_clock::now();
        RunTime = End - Begin;
        cout << "Run Time is " << chrono::duration_cast<chrono::milliseconds>(RunTime).count() << "ms" << endl;
    }
    // hours, microseconds, milliseconds, minutes, nanoseconds, seconds can be used for units
};

mutex m1, m2, m3;
condition_variable cv_buff_part, cv_buff_product;
const int MaxTimePart{30000}, MaxTimeProduct{28000};
vector<int> maxBufferState = {7, 6, 5, 5, 4};
vector<int> currentBufferState(5, 0);
vector<int> manufactureWait = {500, 500, 600, 600, 700};
vector<int> bufferWait = {200, 200, 300, 300, 400};
vector<int> assemblyWait = {600, 600, 700, 700, 800};

vector<int> ProduceLoadOrder(int id, vector<int> unloadedOrders);

vector<int> ProducePickupOrder(int id, vector<int> &unpickedOrders, vector<int> &localState,vector<int> &cartState);

bool CheckBufferStatePart(vector<int> &currentBufferState, vector<int> &loadOrder);

void LoadBufferState(int id, vector<int> &loadOrder, int &iteration);

bool CheckBufferStateProduct(vector<int> &currentBufferState, vector<int> &pickUpOrder);

void UnloadBufferState(int id, vector<int> &pickUpOrder, int &iteration, vector<int> &localState, vector<int> &cartState);

void ProductWorker(int id);

void PartWorker(int id);


void AssembleParts(int id, vector<int> &vector);

int main() {

    const int m = 1, n = 1; //m: number of Part Workers
    //n: number of Product Workers
    //Different numbers might be used during grading.
    vector<thread> PartW, ProductW;
    {
        Timer TT;
        for (int i = 0; i < m; ++i) {

            PartW.emplace_back(PartWorker, i + 1);
        }
        for (int i = 0; i < n; ++i) {
            ProductW.emplace_back(ProductWorker, i + 1);
        }
        for (auto &i: PartW) i.join();
        for (auto &i: ProductW) i.join();
    }
    cout << "Finish!" << endl;

    return 0;
}


vector<int> ProduceLoadOrder(int id, vector<int> unloadedOrders) {

    vector<int> loadOrder(unloadedOrders);
    int fixedOrderItemLimit = 6;
    int presentOrderItems = 0;
    for (auto i: unloadedOrders) {
        presentOrderItems += i;
    }

    int remainingParts = fixedOrderItemLimit - presentOrderItems;
    while (remainingParts != 0) {
        srand(system_clock::now().time_since_epoch().count());
        int randNumParts = rand() % (remainingParts);
        int partType = rand() % 5;
        this_thread::sleep_for(randNumParts * microseconds(manufactureWait[partType]));
        loadOrder[partType] += ++randNumParts;
        remainingParts -= randNumParts;
    }
    return loadOrder;

}

vector<int> ProducePickupOrder(int id, vector<int> &unpickedOrders, vector<int> &localState,vector<int> &cartState) {
    vector<int> pickUpOrder(unpickedOrders);
    localState = unpickedOrders;
    int fixedOrderItemLimit = 5;
    int presentOrderItems = 0;
    unordered_map<int, int> partNum;
    for (int i = 0; i < unpickedOrders.size(); i++) {
        presentOrderItems += unpickedOrders[i];
        if (unpickedOrders[i] > 0)partNum[i]++;
    }

    if(presentOrderItems==0){
        cartState = vector<int>(5,0);
    }
    int remainingParts = fixedOrderItemLimit - presentOrderItems;
    while (remainingParts != 0) {
        srand(system_clock::now().time_since_epoch().count());
        int randNumParts = rand() % (remainingParts);
        int partType = rand() % 5;
        if (partNum.size() <= 3 || (partNum.size() <= 3 && partNum.find(partType) != partNum.end())) {

            if ((partNum.size() == 3 || partNum.size() == 2) && partNum.find(partType) != partNum.end()) {
                pickUpOrder[partType] += ++randNumParts;
                remainingParts -= randNumParts;
                this_thread::sleep_for(microseconds(manufactureWait[partType]) * randNumParts);
            } else if (partNum.size() == 2 && partNum.find(partType) == partNum.end()) {
                partNum[partType]++;
                pickUpOrder[partType] += ++randNumParts;
                remainingParts -= randNumParts;
                this_thread::sleep_for(microseconds(manufactureWait[partType]) * randNumParts);
            } else if (pickUpOrder[partType] + randNumParts + 1 != fixedOrderItemLimit) {
                pickUpOrder[partType] += ++randNumParts;
                remainingParts -= randNumParts;
                this_thread::sleep_for(microseconds(manufactureWait[partType]) * randNumParts);
            }
        }
    }
    for(int i=0;i<pickUpOrder.size();i++){
        pickUpOrder[i] = pickUpOrder[i]-unpickedOrders[i];
    }
    return pickUpOrder;
}


bool CheckBufferStatePart(vector<int> &currentBufferState, vector<int> &loadOrder) {
    for (int i = 0; i < currentBufferState.size(); ++i) {
        if (currentBufferState[i] < maxBufferState[i] && loadOrder[i] > 0) {
            return true;
        }
    }
    return false;
}

bool checkOrder(vector<int> &order) {
    for (auto i: order) {
        if (i > 0)return true;
    }
    return false;
}

void LoadBufferState(int id, vector<int> &loadOrder, int &iteration) {

    loadOrder = ProduceLoadOrder(id, loadOrder);

    //unique_lock UL1(m1);

    auto deadline = steady_clock::now() + microseconds(MaxTimePart);

    while (checkOrder(loadOrder)) {
        unique_lock UL1(m1);
        if (cv_buff_part.wait_until(UL1, deadline,[&loadOrder]() {
            return CheckBufferStatePart(currentBufferState, loadOrder); })) {
            {
                lock_guard LG1(m2);
                cout << endl;
                cout << "Iteration for part worker: " << id << " is : " << iteration + 1 << endl;
                cout << "part worker id " << id;
                cout << " loadOrder value: ";
                for (auto i: loadOrder) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "CurrentBufferState: ";
                for (auto i: currentBufferState) {
                    cout << i << " ";
                }
                cout << endl;
            }
            for (int i = 0; i < currentBufferState.size(); i++) {
                int remainingSpace = maxBufferState[i] - currentBufferState[i];
                if (remainingSpace > 0 && loadOrder[i] > 0) {
                    int amount_to_load = min(remainingSpace, loadOrder[i]);
                    this_thread::sleep_for(microseconds(bufferWait[i]) * amount_to_load);
                    currentBufferState[i] += amount_to_load;
                    loadOrder[i] -= amount_to_load;
                }
            }
            {
                lock_guard LG2(m2);
                cout << "UpdatedBufferState: ";
                for (auto i: currentBufferState) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "loadOrder Updated ";
                for (auto i: loadOrder) {
                    cout << i << " ";
                }
                cout << endl;
            }

        } else {
            {
                lock_guard LG1(m2);
                cout << "Timeout Occurred for part worker: " << id << endl;
            }

            if (iteration < 4) {
                UL1.unlock();
                LoadBufferState(id, loadOrder, ++iteration);
            }
            return;
        }
    }
    cv_buff_part.notify_all();
    cv_buff_product.notify_all();
    if (iteration < 4) {
       // UL1.unlock();
        LoadBufferState(id, loadOrder, ++iteration);
    } else {
        lock_guard LG2(m2);
        cout << "Cannot update again: part worker : " << id << endl;
    }
}

bool CheckBufferStateProduct(vector<int> &currentBufferState, vector<int> &pickUpOrder) {
    for (int i = 0; i < currentBufferState.size(); ++i) {
        if (currentBufferState[i] > 0 && pickUpOrder[i] > 0) {
            return true;
        }
    }
    return false;
}

void
UnloadBufferState(int id, vector<int> &pickUpOrder, int &iteration, vector<int> &localState, vector<int> &cartState) {

    pickUpOrder = ProducePickupOrder(id, pickUpOrder, localState,cartState);

    auto deadline = steady_clock::now() + microseconds(MaxTimeProduct);

    while (checkOrder(pickUpOrder)) {
        unique_lock UL1(m1);
        if (cv_buff_part.wait_until(UL1, deadline, [&pickUpOrder]() {
            return CheckBufferStateProduct(currentBufferState, pickUpOrder);
        })) {
            {
                lock_guard LG1(m2);
                cout << endl;
                cout << "Iteration for product worker: " << id << " is : " << iteration + 1 << endl;
                cout << "product worker id " << id;
                cout << " pickUpOrder value: ";
                for (auto i: pickUpOrder) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "CurrentBufferState: ";
                for (auto i: currentBufferState) {
                    cout << i << " ";
                }
                cout << endl;
            }
            for (int i = 0; i < currentBufferState.size(); i++) {
                int availableSpace = currentBufferState[i];
                if (availableSpace > 0 && pickUpOrder[i] > 0) {
                    int amount_to_unload = min(availableSpace, pickUpOrder[i]);
                    this_thread::sleep_for(microseconds(bufferWait[i]) * amount_to_unload);
                    currentBufferState[i] -= amount_to_unload;
                    pickUpOrder[i] -= amount_to_unload;
                    cartState[i] += amount_to_unload;
                }
            }
            {
                lock_guard LG2(m2);
                cout << "UpdatedBufferState: ";
                for (auto i: currentBufferState) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "pickUpOrder Updated ";
                for (auto i: pickUpOrder) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "cartState Updated ";
                for (auto i: cartState) {
                    cout << i << " ";
                }
                cout << endl;
                cout << "LocalState Updated ";
                for (auto i: localState) {
                    cout << i << " ";
                }
                cout << endl;

            }
            cv_buff_part.notify_all();
            cv_buff_product.notify_all();
        } else {
            {
                lock_guard LG1(m2);
                cout << "Timeout Occurred for product worker: " << id << endl;
            }

            if (iteration < 4) {
                UL1.unlock();
                UnloadBufferState(id, cartState, ++iteration, localState, cartState);
            }
            return;
        }
    }
    AssembleParts(id, cartState);
    cv_buff_part.notify_all();
    cv_buff_product.notify_all();
    //UL1.unlock();
    if (iteration < 4) {
        UnloadBufferState(id, pickUpOrder, ++iteration, localState, cartState);
    } else {
        lock_guard LG2(m2);
        cout << "Cannot update again: product worker : " << id << endl;
    }

}

void AssembleParts(int id, vector<int> &cartState) {
    for (int i = 0; i < cartState.size(); i++) {
        this_thread::sleep_for(microseconds(assemblyWait[i]) * cartState[i]);
        cartState[i]=0;
    }
    lock_guard LG2(m2);
    cout << "Part assembled and created for productWorker: " << id << endl;
}


void ProductWorker(int id) {

    vector<int> pickUpOrder(5), localState(5, 0), cartState(5, 0);
    int iteration = 0;
    UnloadBufferState(id, pickUpOrder, iteration, localState, cartState);

}

void PartWorker(int id) {

    vector<int> loadOrder(5);
    int iteration = 0;
    LoadBufferState(id, loadOrder, iteration);

}