"""Export the actual compiled tuning source; documentation never supplies runtime defaults."""
from pathlib import Path
import re,csv,json
root=Path(__file__).resolve().parents[1]
source=(root/'Source/TroubleSquad/GeneralWeaponTuning.cpp').read_text(encoding='utf-8')
names=['상어잡이 해양 기뢰','최후의 도시 대중교통','칼날 부메랑','발자국 중독 장치','사자의 비가','귀여운 발사기','메아리치는 박쥐칼날','티·버','개틀링 토끼 건','동물 지뢰','타오르는 단궁','전투 토끼 석궁','토끼 초강력 폭발','얼음작렬 갑옷','스태틱의 검','광휘 역장','소용돌이 장갑','회오리 칼날','유미봇','절멸자']
entries=re.findall(r'\{\{([^}]+)\},\{([^}]+)\},\{([^}]+)\},([^}]+)\}',source)
assert len(entries)==len(names)==20
rows=[]
for i,(damage,cooldown,count,meta) in enumerate(entries):
    values=lambda text:[float(v.strip().removesuffix('f')) for v in text.split(',')]
    m=[v.strip().removesuffix('f') for v in meta.split(',')]
    rows.append(dict(id=i,name=names[i],damage=values(damage),cooldown=values(cooldown),count=[int(v) for v in count.split(',')],
        range=float(m[0]),radius=float(m[1]),duration=float(m[2]),interval=float(m[3]),projectile_stat=m[4]=='true',
        evolution_damage=float(m[5]),evolution_cooldown=float(m[6])))
docs=root/'Docs';docs.mkdir(exist_ok=True)
(docs/'GeneralWeaponTuning.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
with (docs/'GeneralWeaponTuning.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.writer(f)
    writer.writerow(['무기']+[f'피해계수_L{i}' for i in range(1,6)]+[f'쿨타임_L{i}' for i in range(1,6)]+[f'발사수_L{i}' for i in range(1,6)]+['사거리','범위','지속시간','틱간격','투사체능력치적용','진화피해배율','진화쿨타임배율'])
    for r in rows:writer.writerow([r['name']]+r['damage']+r['cooldown']+r['count']+[r[k] for k in ['range','radius','duration','interval','projectile_stat','evolution_damage','evolution_cooldown']])
fmt=lambda seq:'/'.join(f'{n:g}' for n in seq)
text='''# 일반 무기 기본 수치 · 영상 참고 추정안

2026-11-19. 원본 자료에서 비어 있던 일반 무기 수치를 플레이용으로 설정했다. 원작 공식 수치를 복원한 표가 아니다.
기준 영상: https://youtu.be/TaGA8xu5LFc · https://youtu.be/n_9mwZX9zgU
실제 런타임의 원본은 `GeneralWeaponTuning.cpp`이며 이 표와 CSV/JSON은 그 소스에서 추출한다.

레벨 1→5 순서다. 쿨타임은 초, 거리는 현재 프로젝트 월드 단위다. 피해계수에는 능력치·치명타·방어력·최대 체력 계수가 아직 적용되지 않았다.
발사수는 무기별로 탄 수, 방향당 탄 수, 연쇄 횟수, 포격 횟수 또는 회전 칼날 수를 뜻한다.

| 무기 | 피해계수 1→5 | 쿨타임 1→5 | 기본 발사/타격수 1→5 | 사거리 | 범위 | 지속시간 | 투사체 능력치 |
|---|---|---|---|---:|---:|---:|---|
'''
for r in rows:
    text+=f"| {r['name']} | {fmt(r['damage'])} | {fmt(r['cooldown'])} | {fmt(r['count'])} | {r['range']:g} | {r['radius']:g} | {r['duration']:g} | {'적용' if r['projectile_stat'] else '미적용'} |\n"
text+='''
## 실제 피해 방식

- 기뢰·부메랑·초승달·발사기·폭탄·번개는 기본 피해계수를 사용한다. 관통·연쇄·회수·진화 추가 타격은 별도다.
- 열차: 기본 피해계수 + 방어력×1.5. 진화 폭발은 기본 피해계수×0.65.
- 독성 흔적: 틱마다 기본 피해계수×0.33. 티버: ×0.8. 개틀링·화염: ×0.38.
- 석궁: 탄마다 ×0.66. 포격 진화의 마지막 타격: ×2.3.
- 얼음 갑옷: 기본 피해계수 + 방어력×1.5 + 최대 체력×0.035.
- 광휘 역장: 기본 피해계수×0.35 + 최대 체력×0.025. 반경에는 레벨당 12가 더해진다.
- 소용돌이 장갑: 탄마다 ×0.4. 회오리 칼날·유미봇: 틱마다 ×0.65.
- 절멸자: 일반 적 즉사, 정예 등에는 기본 피해계수×8. 최종 보스는 대상에서 제외한다.
- 발자국·광휘 역장·소용돌이 장갑의 기본 발동 간격에는 스킬 가속을 적용하지 않는다.
- 진화 시 피해·쿨타임 배율은 JSON/CSV에 기록했다. 부메랑은 기본 4개, 석궁은 추가 30발,
  포격은 추가 2회, 장갑은 두 줄기, 회오리 칼날은 영구 지속 등 기존 진화 동작도 적용한다.

## 조정 의도와 다음 확인

귀여운 발사기는 빠른 단일 대상 공격, 기뢰·폭탄·포격은 군중 처리, 얼음 갑옷·회오리·유미봇은 생존 보조 역할이다.
같은 무기를 다시 골랐을 때 모든 무기가 무조건 탄을 하나 늘리는 방식은 아니다. 표에 정한 레벨 성장과 별도 투사체 능력치를 합산한다.
실제 비행 탄을 쓰는 무기는 비행 중 적과 충돌할 때 피해를 주며 발사 순간에 피해를 주지 않는다.
15분 완주·최종 보스 처치 시간·무기 조합별 성능은 이 초안의 추가 밸런스 검증 항목이다.
'''
(docs/'GeneralWeaponTuning.md').write_text(text,encoding='utf-8')
print('Exported 20 weapons, 5 ranks each, base and evolution multipliers.')
