# 맵 배치 축·분화구 면 수정

2026-11-20 개발본 변경이며 공유 테스트 패키지에는 미포함.

원인: Unreal Python Rotator의 위치 인수 사용으로 수평 회전 의도가 pitch에 적용됐다. ImproveReferenceArenas.py를 pitch/yaw/roll 이름 인수로 수정하고 작성된 배치값을 다시 적용했다. 임시 Euler 역변환에 의존하지 않는다.

Crater 메시의 면 winding도 반대여서 위에서 보이지 않았다. 사각 면 인덱스 순서를 수정하고 Blender FBX를 다시 임포트했다.

수정 전 기울어진 배치: CoastalResearch14, Outskirts206, SubterraneanLab208, Beachhead140 (총568).
검증 후: Reference polish / Architecture 배치 총766개, pitch/roll 오류0. 외곽·상륙지 각3개의 분화구 minZ=0, maxZ>50 검사 통과. 실제 수치는 Saved/Logs/ArenaPlacementAcceptance.json.

근거: ArenaPlacementAcceptance.log의 ARENA_PLACEMENT_ACCEPTANCE_PASS, CorrectedArenaVisuals.log 및 FeedbackReview/Map_* 렌더.
전체 구조의 원작 대비 일치도나 보스 완주를 이 검사만으로 통과 처리하지 않는다. 수정한 맵의 보스 경로·장시간 전투 검증은 남아 있다.
