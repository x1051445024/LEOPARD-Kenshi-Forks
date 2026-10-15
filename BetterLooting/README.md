# BetterLooting

BetterLooting is a native RE_Kenshi plugin for Kenshi.

This repository contains the corresponding source code for the distributed plugin build.

## Source layout

- Source/BetterLooting.cpp
- Source/BetterLootingRules.h
- Source/BetterLooting.vcxproj

## Build

Build the project in Release x64 with the compatible MSVC toolchain and the RE_Kenshi/KenshiLib development dependencies required by the project configuration.

## License

This project is distributed under the GNU General Public License version 3. See LICENSE.txt.

## Изменения форка: MCM и журнал (04.10.2026)
- Настройки — ещё и во вкладке MCM окна «Настройки» (мод ModConfigMenu): клавиша сбора, радиус, правила, категории. Пишутся в тот же BetterLooting.ini и применяются сразу. Своего окна (SHIFT+L) больше нет - см. ниже.
- Журнал: сбои (FAIL, ERROR, fault, failed) пишутся всегда, остальные подробности — только при `[Debug] Log=1` в BetterLooting.ini или «Подробный журнал» в MCM. Раньше на каждое открытие инвентаря в журнал шла строка «Error … CLASS source=…».
- Ini читается тем же путём, что и пишется: рядом с DLL.
- Перевод: en/ru/zh (locale в папке мода).
- На странице MCM - кнопки профилей (баланс, деньги, ресурсы) и «Все категории» / «Ни одной», как в своей панели; применяются и сохраняются сразу.
- Окно настроек на SHIFT+L и сама клавиша убраны (04.10.2026): все настройки, профили и «Все/Ни одной категории» - на странице MCM. Вместе с окном ушла кнопка «Разложить всё».

## Выкладка форка (05.10.2026)
- Версия 1 этого форка - для Steam и Nexus («Better Looting (MCM fork) (QoL)»). Оригинал - XxAtreuSSxX (страница в Steam снята автором), GPLv3.
- В проект `BetterLooting.vcxproj` добавлены `BetterLootingPanel.cpp` и заголовки - раньше в нём был только `BetterLooting.cpp`, и сборка по проекту не находила пресеты.
- Клавиша сбора в MCM назначается нажатием: ключ `[Behavior] ActivationKey` (имя клавиши, `shared/HoldKey.h`); если он есть - главнее старого числового `ActivationVirtualKey`.
- В раздачу не идёт `BetterLooting.ini` (личные настройки): без него действуют значения по умолчанию (Insert, 20 м, от 500 и от 300 за кг), файл появится при первом сохранении настроек.
- Исходники - архив `BetterLooting-source.zip` в папке мода (без файлов-бэкапов `*.pre*` наших патчей) и папка `shared` с общими заголовками (Localization.h, ModConfigMenu.h).

## Версия 2: деньги и краденое (14-15.10.2026)
- Деньги подбираются: предметы с функцией `ITEM_MONEY` (Cats, String of Cats из игры, Rope of Cats из модов) у мёртвых и потерявших сознание людей превращаются в деньги игрока, в рюкзак они не кладутся. Отдельная функция `TakeBetterLootingMoney`: `Ownerships::addMoney` у взвода собирающего персонажа со сверкой через `getMoney()`, потом вся стопка снимается с тела вызовом игры `Inventory::removeItemAutoDestroy`; если что-то не сошлось - зачисление откатывается, а деньги выключаются до перезапуска (в журнале `MONEY_FAIL`).
- Деньги больше никогда не идут через общий перенос предметов (`IsAllowedLootCategory` отказывает `ITEM_MONEY` - и при сборе, и при «разложить всё»). Первая попытка 14.10 возила их как предметы: проверка переноса не сходилась, тот же `Item` попадал во второй инвентарь игрока, и следующее окно инвентаря падало в `InventorySection::recalculateTotalWeight`. До 14.10 деньги попадали туда же через категорию «Прочее».
- Из сундуков и построек деньги не берутся: с чужого склада это была бы кража, которую никто не увидит.
- Настройка `[Rules] TakeMoney` (по умолчанию 1), в MCM - «Брать деньги» в разделе «Правила». У живого без сознания и помеченные краденым - только при `AllowStolen=1`.
- Краденым считается только взятое у живого человека без сознания (игра сама показывает там шанс кражи). Мёртвые, животные и роботы-животные хозяина не имеют - добыча с охоты и поля боя больше не помечается краденой.
