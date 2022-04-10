#### Сборка:

* Дебаг режим: `./build_from_scratch.sh -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZER_ADDRESS=True`

* Релиз режим: `./build_from_scratch.sh -DCMAKE_BUILD_TYPE=Release`

Первая сборка займёт сильно больше времени, потому что подтянет библиотеки (всё локально, на систему ничего не ставится)



Последующие сборки можно выполнять `./build.sh`


