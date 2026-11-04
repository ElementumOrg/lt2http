import { Button, Checkbox, Group, Modal, Stack, Text } from '@mantine/core';
import { useState } from 'react';
import { api, TorrentInfo } from '../api';
import { useI18n } from '../i18n';
import { run } from './run';

interface Props {
  torrent: TorrentInfo | null;
  onClose: () => void;
  onRemoved?: () => void;
}

// Removes a torrent: from the session only, or with its torrent information and/or downloaded data
export function RemoveModal({ torrent, onClose, onRemoved }: Props) {
  const { t } = useI18n();
  const [deleteFiles, setDeleteFiles] = useState(true);
  const [deleteData, setDeleteData] = useState(false);

  const remove = () =>
    torrent &&
    run(
      () => api.remove(torrent.hash, deleteFiles, deleteData && !torrent.is_memory_storage),
      t('remove.done'),
      () => {
        onClose();
        onRemoved?.();
      },
    );

  return (
    <Modal opened={torrent !== null} onClose={onClose} title={t('remove.title')} centered>
      <Stack>
        <Text fw={500}>{torrent?.name}</Text>
        <Checkbox
          label={t('remove.torrent_files')}
          checked={deleteFiles}
          onChange={(e) => setDeleteFiles(e.currentTarget.checked)}
        />
        {!torrent?.is_memory_storage && (
          <Checkbox
            label={t('remove.data')}
            checked={deleteData}
            onChange={(e) => setDeleteData(e.currentTarget.checked)}
          />
        )}
        <Text size="sm" c="dimmed">
          {t('remove.hint')}
        </Text>
        <Group justify="flex-end">
          <Button variant="default" onClick={onClose}>
            {t('ui.cancel')}
          </Button>
          <Button color="red" onClick={remove}>
            {t('action.remove')}
          </Button>
        </Group>
      </Stack>
    </Modal>
  );
}
