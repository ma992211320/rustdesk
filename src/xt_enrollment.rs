use hbb_common::{config::Config, sodiumoxide::crypto::sign};

// Never return the engine private key to the webview or the box process.
fn sign_challenge(user_id: u64, device_id: &str, nonce: &str, expires: u64) -> Result<String, &'static str> {
    let now = (hbb_common::get_time() / 1000) as u64;
    let message = challenge_message(user_id, device_id, nonce, expires, now)?;
    if !Config::get_key_confirmed() || Config::get_id() != device_id {
        return Err("device identity not confirmed");
    }
    let pair = Config::get_key_pair();
    let secret = sign::SecretKey::from_slice(&pair.0).ok_or("invalid device key")?;
    let signature = sign::sign_detached(message.as_bytes(), &secret);
    Ok(crate::encode64(signature.to_bytes()))
}

pub fn sign_from_stdin() -> Result<String, &'static str> {
    use std::io::Read;
    #[derive(serde::Deserialize)]
    #[serde(deny_unknown_fields)]
    struct Request {
        account_id: u64,
        device_id: String,
        nonce: String,
        expires_at: u64,
    }
    let mut input = Vec::new();
    std::io::stdin().lock().take(1025).read_to_end(&mut input)
        .map_err(|_| "cannot read challenge")?;
    if input.len() > 1024 {
        return Err("challenge too large");
    }
    let request: Request = serde_json::from_slice(&input).map_err(|_| "invalid challenge")?;
    sign_challenge(request.account_id, &request.device_id, &request.nonce, request.expires_at)
}

fn challenge_message(user_id: u64, device_id: &str, nonce: &str, expires: u64, now: u64) -> Result<String, &'static str> {
    if user_id == 0 || user_id > i64::MAX as u64
        || !(6..=16).contains(&device_id.len()) || !device_id.bytes().all(|b| b.is_ascii_digit())
        || nonce.len() != 64 || !nonce.bytes().all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        || expires <= now || expires.saturating_sub(now) > 120 {
        return Err("invalid enrollment challenge");
    }
    Ok(format!("XT-REMOTE-ENROLL-V1\n{user_id}\n{device_id}\n{nonce}\n{expires}"))
}

#[cfg(test)]
mod tests {
    use super::challenge_message;
    #[test]
    fn matches_server_contract() {
        let nonce = "a".repeat(64);
        assert_eq!(challenge_message(7,"123456789",&nonce,1800000090,1800000000).unwrap(),
            format!("XT-REMOTE-ENROLL-V1\n7\n123456789\n{nonce}\n1800000090"));
        assert!(challenge_message(0,"123456789",&nonce,1800000090,1800000000).is_err());
        assert!(challenge_message(7,"123456789",&nonce,1800000000,1800000000).is_err());
        assert!(challenge_message(7,"123456789",&nonce,1800000121,1800000000).is_err());
    }
}
