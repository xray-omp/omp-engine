# X-RAY OMP
X-RAY OMP - проект, представляющий собой набор правок движка/скриптов игры S.T.A.L.K.E.R.: Зов Припяти для мультиплеера. Проект создан с целью поделиться некоторыми наработками с другими разработчиками мультиплеерных проектов.

## gamedata
gamedata находится в [отдельном репозитории](https://github.com/xray-omp/omp-gamedata).

## Сборка
* [Скачать](https://visualstudio.microsoft.com/ru/) Viusal Studio 2019 (достаточно Community версии).
* Установить Visual Studio. При установке выбрать Рабочие нагрузки -> Разработка классических приложений на C++.
(Необходимо наличие MSVC версии 142 и пакет SDK для Windows 10)
* [Скачать](https://www.microsoft.com/en-us/download/details.aspx?id=6812) и установить DirectX SDK.
* Открыть src/engine.sln
* Меняем конфигурацию сборки на Release\* и собираем!

\* Debug сборка также возможна. Mixed сборка не настроена! Release_Dedicated и Debug_Dedicated предназначены для выделенного сервера, на них собирается только xrEngine.
