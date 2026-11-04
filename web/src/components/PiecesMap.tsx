import { Badge, Box, Group, Paper, ScrollArea, Text } from '@mantine/core';
import { useElementSize } from '@mantine/hooks';
import { MouseEvent, useEffect, useMemo, useRef, useState } from 'react';
import { FileInfo } from '../api';
import { useI18n } from '../i18n';

// States of the characters of Pieces.map, digits 1-7 are queued pieces
const STATES = [
  { chars: '+', color: 'var(--mantine-color-green-6)', label: 'pieces.stored' },
  { chars: '*', color: 'var(--mantine-color-blue-6)', label: 'pieces.read' },
  { chars: '?', color: 'var(--mantine-color-orange-6)', label: 'pieces.wanted' },
  { chars: '~', color: 'var(--mantine-color-cyan-5)', label: 'pieces.downloading' },
  { chars: '1234567', color: 'var(--mantine-color-gray-5)', label: 'pieces.queued' },
  { chars: '-', color: 'var(--mantine-color-default-border)', label: 'pieces.not_wanted' },
];
const stateOf = (c: string) => STATES.find((s) => s.chars.includes(c)) ?? STATES[4];

const GAP = 2;
const MAX_HEIGHT = 420;

// Squares big enough to see and point at: as big as fits MAX_HEIGHT, from 8 to 18 pixels
function cellSize(width: number, count: number): number {
  const fit = Math.floor(Math.sqrt((width * MAX_HEIGHT) / Math.max(1, count)));
  return Math.min(18, Math.max(8, fit));
}

interface Props {
  map: string;
  pieceLength: number;
  files: FileInfo[];
}

export function PiecesMap({ map, pieceLength, files }: Props) {
  const { t } = useI18n();
  const { ref, width } = useElementSize();
  const canvas = useRef<HTMLCanvasElement>(null);
  const [hover, setHover] = useState<{ index: number; x: number; y: number }>();

  const size = cellSize(width, map.length);
  const perRow = Math.max(1, Math.floor((width + GAP) / (size + GAP)));
  const height = Math.ceil(map.length / perRow) * (size + GAP);

  const counts = useMemo(() => {
    const result = new Map<string, number>();
    for (const c of map) {
      const label = stateOf(c).label;
      result.set(label, (result.get(label) ?? 0) + 1);
    }
    return result;
  }, [map]);

  useEffect(() => {
    const element = canvas.current;
    if (!element || !width) return;
    // Sharp on high density screens
    const ratio = window.devicePixelRatio || 1;
    element.width = width * ratio;
    element.height = height * ratio;
    element.style.width = `${width}px`;
    element.style.height = `${height}px`;

    // Canvas takes no CSS variables: resolve them on the element
    const style = getComputedStyle(element);
    const colors = new Map(STATES.map((s) => [s.label, style.getPropertyValue(s.color.slice(4, -1)).trim()]));

    const ctx = element.getContext('2d')!;
    ctx.scale(ratio, ratio);
    for (let i = 0; i < map.length; i++) {
      ctx.fillStyle = colors.get(stateOf(map[i]).label)!;
      ctx.beginPath();
      ctx.roundRect((i % perRow) * (size + GAP), Math.floor(i / perRow) * (size + GAP), size, size, 2);
      ctx.fill();
    }
  }, [map, width, height, size, perRow]);

  const onMove = (event: MouseEvent<HTMLCanvasElement>) => {
    const rect = event.currentTarget.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    const index = Math.floor(y / (size + GAP)) * perRow + Math.floor(x / (size + GAP));
    setHover(index >= 0 && index < map.length && x < perRow * (size + GAP) ? { index, x, y } : undefined);
  };

  // The files a piece has data of
  const filesOf = (index: number) =>
    pieceLength > 0
      ? files.filter(
          (f) =>
            f.size > 0 &&
            index >= Math.floor(f.offset / pieceLength) &&
            index <= Math.floor((f.offset + f.size - 1) / pieceLength),
        )
      : [];

  return (
    <div>
      <Group gap="xs" mb="sm">
        {STATES.map((s) => (
          <Badge
            key={s.label}
            variant="light"
            color="gray"
            leftSection={
              <span style={{ display: 'inline-block', width: 10, height: 10, borderRadius: 2, background: s.color }} />
            }
          >
            {t(s.label)}: {counts.get(s.label) ?? 0}
          </Badge>
        ))}
      </Group>
      <ScrollArea.Autosize mah={MAX_HEIGHT + 40} type="auto">
        <Box ref={ref} pos="relative">
          <canvas
            ref={canvas}
            style={{ display: 'block', cursor: 'crosshair' }}
            onMouseMove={onMove}
            onMouseLeave={() => setHover(undefined)}
          />
          {hover && (
            <Paper
              shadow="md"
              withBorder
              p={6}
              pos="absolute"
              style={{
                left: Math.min(hover.x + 12, Math.max(0, width - 260)),
                top: hover.y + 14,
                pointerEvents: 'none',
                zIndex: 1,
                maxWidth: 260,
              }}
            >
              <Text size="xs" fw={600}>
                {t('pieces.piece', hover.index)} · {t(stateOf(map[hover.index]).label)}
              </Text>
              {filesOf(hover.index).map((f) => (
                <Text key={f.id} size="xs" c="dimmed" style={{ overflowWrap: 'anywhere' }}>
                  {f.path}
                </Text>
              ))}
            </Paper>
          )}
        </Box>
      </ScrollArea.Autosize>
    </div>
  );
}
